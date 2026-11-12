// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/ShotRequest.h"
#include "Combat/ShotResult.h"
#include "Networking/NetworkValidationTypes.h"
#include "HitValidationComponent.generated.h"

class UGameplayEffect;
class UCombatComponent;
class ULagCompensationComponent;
struct FHistoricalHitbox;

/*
 * 服务器命中校验统一入口（M6）
 * 职责：校验射击请求 -> 时间窗口 -> 历史 hitbox 回溯 trace -> 构造权威 FShotResult -> 命中则应用伤害 GE
 * 不播 UI、不做属性结算（属性在 AttributeSet），不扣弹（扣弹在 CombatComponent，接受后才扣）
 * 只在服务器上有意义；客户端上这个组件什么都不做
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UHitValidationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitValidationComponent();

	UFUNCTION(BlueprintPure, Category = "FPS|HitValidation")
	static UHitValidationComponent* FindHitValidationComponent(const AActor* Actor) { return IsValid(Actor) ? Actor->FindComponentByClass<UHitValidationComponent>() : nullptr; }

	/*
	 * 唯一入口：由 CombatComponent::Server_RequestFire_Implementation 调用
	 * RequestingController 必须来自服务器已知的拥有关系（Pawn->GetController()），不能是客户端传来的指针
	 * 返回的结果 bAccepted=false 时调用方不得扣弹/广播命中
	 */
	FShotResult ValidateAndProcessShot(const FShotRequest& Request, AController* RequestingController);

	//1~5 步静态校验：射手/武器/弹药/状态/序列号/方向/起点（不含时间与射速）
	bool ValidateShotRequest(const FShotRequest& Request, AController* RequestingController, EShotRejectReason& OutRejectReason) const;

	//当前世界 trace（未开 SSR 或回溯数据不可用时的降级路径，也用于对比诊断）
	bool PerformCurrentWorldTrace(const FShotRequest& Request, FHitResult& OutHit) const;

	//历史 hitbox 回溯 trace：命中写进 OutResult（HitActor/HitboxName/ImpactPoint/SurfaceType）
	bool PerformRewindTrace(const FShotRequest& Request, double ValidatedServerShotTime, FShotResult& OutResult) const;

	//服务器应用伤害 GE；返回 true 表示这一下致死（来自 GAS 死亡判定）
	bool ApplyDamageGameplayEffect(AActor* Target, float Damage, const FHitResult* Hit) const;

	//伤害 GE，默认 UGE_Damage（SetByCaller Data.Damage）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitValidation")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	//TraceOrigin 与服务器眼睛位置允许的偏差（cm）；要覆盖移动预测造成的 RTT/2 位置差
	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitValidation")
	float OriginTolerance;

	//射速容差：两发间隔 >= FireTime * 该值 才接受（吸收时钟估计抖动）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitValidation")
	float FireRateTolerance;

	//爆头伤害倍率（1=与旧代码一致，不加成）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitValidation")
	float HeadShotMultiplier;

	//回溯粗筛半径（cm）：射线离目标 Actor 原点超过这个距离直接跳过，不逐个盒子算
	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitValidation")
	float RewindBroadPhaseRadius;

private:

	FShotResult& RejectShot(FShotResult& Result, EShotRejectReason Reason) const;

	//§17.1 诊断字段日志（fps.Shot.Log 控制）。EstimatedOffsetMs 为服务器时间戳容差（毫秒），见实现注释
	void LogShot(const FShotRequest& Request, const FShotResult& Result, double ServerReceiveTime, float EstimatedOffsetMs) const;

	//射线 vs 有向盒（盒子尺寸按 TraceRadius 膨胀近似球形 sweep），命中返回进入距离
	static bool RayIntersectsHitbox(const FVector& Start, const FVector& Dir, float MaxDistance, const FHistoricalHitbox& Box, float Inflate, float& OutDistance);

	//S2-4：服务器对射击方向施加确定性散布扰动（用 Request.SpreadSeed 复现客户端预测），返回扰动后的方向
	FVector ApplySpreadToDirection(const FShotRequest& Request, const FVector& BaseDirection) const;

	//S2-2：计算当前姿态/移动/连发状态下的散布半角（度），读 UWeaponData::WeaponAccuracy
	float ComputeSpreadDegrees(const FShotRequest& Request) const;

	//S3-5：查询射击时刻的烟雾遮挡——返回烟雾遮挡的最近距离，无遮挡返回 -1
	float FindSmokeBlockDistance(const FVector& Start, const FVector& Dir, float MaxDistance, double ShotServerTime) const;

	UCombatComponent* GetCombat() const;

	//已处理过的最大序列号（重复/过旧直接拒）
	uint32 LastProcessedShotSequence;
	bool bHasProcessedShot;

	//上一发已接受射击的服务器时刻（射速、时间倒流校验）
	double LastAcceptedShotTime;
	bool bHasAcceptedShot;
};

// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillSmokeActor.generated.h"

class UStaticMeshComponent;

/*
 * 烟雾技能产物（S3-4）：服务器权威生成、复制位置/半径/生命周期，到期销毁
 * 职责只有「占一块空间挡视线」，命中判定（rewind trace 是否被它挡）在 HitValidationComponent
 * 里按「射击时刻烟雾是否已存在」查询，见 S3-5。
 * 表现（灰盒球体/粒子）走 GameplayCue，不在这里 Spawn 粒子。
 */
UCLASS()
class FPS_API ASkillSmokeActor : public AActor
{
	GENERATED_BODY()

public:
	ASkillSmokeActor();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//服务器初始化烟雾：位置 + 半径 + 持续时长，并启动销毁计时
	void ServerInitSmoke(const FVector& InLocation, float InRadius, float InLifetime);

	//命中判定用：某时刻（服务器时间）这团烟是否已经存在且未过期
	bool IsActiveAtTime(double ServerTime) const;

	//查询遮挡：给定线段，若烟雾球体遮挡了它（穿过球体），返回进入球体的距离
	//OutDistance = 从 Start 沿 Dir 到球面交点的距离；不遮挡返回 false
	bool BlocksLine(const FVector& Start, const FVector& Dir, float MaxDistance, float& OutDistance) const;

	//烟雾中心（世界空间）
	FVector GetSmokeLocation() const { return SmokeLocation; }

	//烟雾半径
	float GetSmokeRadius() const { return SmokeRadius; }

	//生成时刻（服务器世界时间），回溯判定用
	double GetSpawnServerTime() const { return SpawnServerTime; }

	//灰盒表现：半透明球体占位，Scale 跟随 SmokeRadius（正式阶段再换 GameplayCue 烟粒子）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Smoke")
	TObjectPtr<UStaticMeshComponent> SmokeVisual;

private:
	//服务器初始化参数（复制给客户端做表现）
	UPROPERTY(Replicated)
	FVector SmokeLocation;

	UPROPERTY(Replicated)
	float SmokeRadius;

	//烟雾生成时刻 + 到期时刻（服务器世界时间轴）
	double SpawnServerTime;
	double ExpireServerTime;

	//生命周期，到期自动销毁
	FTimerHandle LifetimeHandle;

	void OnLifetimeExpired();
};

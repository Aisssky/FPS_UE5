// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "WeaponData.generated.h"

class UAnimMontage;
class UBlendSpace;
class UAnimSequence;

USTRUCT(BlueprintType)
struct FPlayerAnims
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> IdleAnim = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> AimIdleAnim = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> CrouchIdleAnim = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UAnimSequence> SprintAnim = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> AimOffset_Hip;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> AimOffset_Aim;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> Strafe_Standing;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> Strafe_Crouching;
};

USTRUCT(BlueprintType)
struct FMontageData
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> EquipMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> ReloadMontage = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> FireMontage = nullptr;
};

/*
 * 枪械精度数据（S2-2）：散布角按武器/姿态/移动状态分级
 * 角度单位 = 度；半角（half-angle），代表「最大偏差方向与准星方向的夹角」
 * 服务器施加散布时（D-S2）读这些值；客户端用同值做准星扩散表现（S2-5）
 */
USTRUCT(BlueprintType)
struct FWeaponAccuracy
{
	GENERATED_BODY()

	//站立静止（首发基准散布）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy")
	float BaseSpreadDegrees = 0.5f;

	//移动中的额外散布（加到 Base 上）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy")
	float MoveSpreadDegrees = 2.5f;

	//空中额外散布
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy")
	float AirSpreadDegrees = 4.f;

	//蹲伏倍率（<1 收束）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy")
	float CrouchSpreadMultiplier = 0.6f;

	//瞄准倍率（<1 收束）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy")
	float AimSpreadMultiplier = 0.5f;

	//连发每发累加的散布（度）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy")
	float BloomPerShotDegrees = 0.15f;

	//散布上限（度），封顶避免无限膨胀
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy")
	float MaxBloomDegrees = 3.f;

	//散布回复速度（度/秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accuracy")
	float BloomRecoveryPerSecond = 4.f;
};

/*
 * 固定后坐力弹道（S2-3）：每发的 pitch/yaw 偏移序列
 * 确定性数据：同一把枪每次压枪轨迹一致，服务器与客户端用同一份序列索引，无需同步随机数
 * pitch 正值 = 枪口上抬；yaw 正值 = 向右偏
 */
USTRUCT(BlueprintType)
struct FRecoilPattern
{
	GENERATED_BODY()

	//每发偏移（pitch 度, yaw 度），按下标=连发序号索引
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	TArray<FVector2D> Shots;

	//打完后坐力是否立即回落（true=松手回到原点；false=保持）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	bool bRecoverOnRelease = true;
};

UCLASS()
class FPS_API UWeaponData : public UDataAsset
{
	GENERATED_BODY()
public:
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|WeaponData|Weapons")
	TMap<FGameplayTag, FName> GripPoints;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|Weapons")
	TMap<FGameplayTag, FMontageData> WeaponMontages;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|FirstPerson")
	TMap<FGameplayTag, FPlayerAnims> FirstPersonAnims;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|ThirdPerson")
	TMap<FGameplayTag, FPlayerAnims> ThirdPersonAnims;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|FirstPerson")
	TMap<FGameplayTag, FMontageData> FirstPersonMontages;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|ThirdPerson")
	TMap<FGameplayTag, FMontageData> ThirdPersonMontages;

	//按武器类型配精度（S2-2），键=WeaponType
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|Accuracy")
	TMap<FGameplayTag, FWeaponAccuracy> WeaponAccuracy;

	//按武器类型配固定后坐力弹道（S2-3）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|Recoil")
	TMap<FGameplayTag, FRecoilPattern> WeaponRecoil;

	//S4-4：武器购买价格，键=WeaponType（Weapon.Type.Rifle / Weapon.Type.Pistol），Buy 阶段花 Money 解锁
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|Economy")
	TMap<FGameplayTag, int32> WeaponPrices;
	
};

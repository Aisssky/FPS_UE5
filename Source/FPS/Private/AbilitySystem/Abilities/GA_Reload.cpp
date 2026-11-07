// Copyright Aisssky


#include "AbilitySystem/Abilities/GA_Reload.h"

#include "TimerManager.h"
#include "Combat/CombatComponent.h"
#include "Engine/World.h"
#include "Tags/ShooterGameplayTags.h"
#include "Weapon/Weapon.h"


UGA_Reload::UGA_Reload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	//与 GA_FireWeapon 只在本地跑
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	SetAssetTags(FGameplayTagContainer(ShooterTags::TAG_Ability_Reload));
	ActivationOwnedTags.AddTag(ShooterTags::TAG_State_Reloading);
	ActivationBlockedTags.AddTag(ShooterTags::TAG_State_Reloading);
	ActivationBlockedTags.AddTag(ShooterTags::TAG_State_Dead);

	ReloadTimeout = 5.f;
}

UCombatComponent* UGA_Reload::GetCombat(const FGameplayAbilityActorInfo* ActorInfo)
{
	return ActorInfo ? UCombatComponent::FindCombatComponent(ActorInfo->AvatarActor.Get()) : nullptr;
}

bool UGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	//本地快速过滤：弹匣未满、还有备弹、武器不在切枪/换弹中
	const UCombatComponent* Combat = GetCombat(ActorInfo);
	if (!Combat || !Combat->CurrentWeapon) return false;

	const AWeapon* Weapon = Combat->CurrentWeapon;
	return Weapon->Ammo < Weapon->MagCapacity
		&& Combat->CurrentReserveAmmo > 0
		&& Weapon->WeaponStatus != EWeaponStatus::Cycling
		&& Weapon->WeaponStatus != EWeaponStatus::Reloading;
}

void UGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UCombatComponent* Combat = GetCombat(ActorInfo);
	if (!Combat)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//先绑定再开始换弹：避免单机下同一帧就结束导致漏掉回调
	BoundCombat = Combat;
	ReloadFinishedHandle = Combat->OnLocalReloadFinished.AddUObject(this, &ThisClass::OnReloadFinished);

	//真正开始换弹（本地蒙太奇 + Server_ReloadWeapon）；没开始就直接结束
	if (!Combat->Initiate_ReloadWeapon())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//单机下 Initiate 里可能已经同步结束了，这时不要再开超时
	if (IsActive())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(TimeoutHandle, this, &ThisClass::OnReloadTimeout, ReloadTimeout, false);
		}
	}
}

void UGA_Reload::OnReloadFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Reload::OnReloadTimeout()
{
	UE_LOG(LogTemp, Warning, TEXT("[GAS] GA_Reload timeout, force end"));

	//超时：把本地武器状态也放回 Idle，否则会卡在 Reloading 永远不能开火
	if (UCombatComponent* Combat = BoundCombat.Get(); Combat && Combat->CurrentWeapon && Combat->CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading)
	{
		Combat->CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//清理监听/计时器，保证重复调用安全
	if (UCombatComponent* Combat = BoundCombat.Get())
	{
		Combat->OnLocalReloadFinished.Remove(ReloadFinishedHandle);
	}
	ReloadFinishedHandle.Reset();
	BoundCombat.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

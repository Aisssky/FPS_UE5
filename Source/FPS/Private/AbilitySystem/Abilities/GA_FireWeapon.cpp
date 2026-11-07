// Copyright Aisssky


#include "AbilitySystem/Abilities/GA_FireWeapon.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatComponent.h"
#include "Tags/ShooterGameplayTags.h"
#include "Weapon/Weapon.h"


UGA_FireWeapon::UGA_FireWeapon()
{
	//每个角色一个实例：需要在激活期间保持状态（按住开火）
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	//只在本地跑
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	//自身标签
	SetAssetTags(FGameplayTagContainer(ShooterTags::TAG_Ability_Fire));
	//激活期间给角色挂 State.Firing
	ActivationOwnedTags.AddTag(ShooterTags::TAG_State_Firing);
	//换弹中、死亡时不能开火
	ActivationBlockedTags.AddTag(ShooterTags::TAG_State_Reloading);
	ActivationBlockedTags.AddTag(ShooterTags::TAG_State_Dead);
}

UCombatComponent* UGA_FireWeapon::GetCombat(const FGameplayAbilityActorInfo* ActorInfo)
{
	return ActorInfo ? UCombatComponent::FindCombatComponent(ActorInfo->AvatarActor.Get()) : nullptr;
}

bool UGA_FireWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	const UCombatComponent* Combat = GetCombat(ActorInfo);
	return Combat && Combat->CurrentWeapon;
}

void UGA_FireWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                     const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                     const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UCombatComponent* Combat = GetCombat(ActorInfo);
	if (!Combat || !Combat->CurrentWeapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	/*
	 * 扣扳机：本地表现 + BuildShotRequest + Server_RequestFire 都在这里面
	 * 自动武器由 CombatComponent 的 FireTimer 循环，直到 Initiate_FireWeapon_Released
	 * 这里不 EndAbility：保持激活到松开，State.Firing 才能覆盖整个开火过程
	 */
	
	Combat->Initiate_FireWeapon_Pressed();
}

void UGA_FireWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//松开/取消/死亡都走这里：放开扳机，停止自动连发
	if (UCombatComponent* Combat = GetCombat(ActorInfo))
	{
		Combat->Initiate_FireWeapon_Released();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

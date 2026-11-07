// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_FireWeapon.generated.h"

class UCombatComponent;

/*
 * 射击 Ability：只管生命周期/Tag/取消
 * - 按下开火键激活，松开时结束，期间持有 State.Firing
 * - 半自动/自动、射速计时器、本地表现、构造 FShotRequest 由 CombatComponent 执行
 * - 服务器权威命中在 HitValidationComponent
 * 网络策略 LocalOnly：射击意图已经通过 Server_RequestFire 发给服务器，Ability 本身不需要在服务器激活
 */
UCLASS()
class FPS_API UGA_FireWeapon : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UGA_FireWeapon();

	//检查 Tag/状态
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	//激活
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData)override;

	//结束/取消
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled)override;

private:

	static UCombatComponent* GetCombat(const FGameplayAbilityActorInfo* ActorInfo);
};

// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Reload.generated.h"

class UCombatComponent;

/*
 * 换弹 Ability
 * 流程：验证 -> 挂 State.Reloading -> CombatComponent 播蒙太奇 + Server_ReloadWeapon
 *       -> 等本地换弹结束（动画通知或服务器补弹回包）-> 移除 State.Reloading -> EndAbility
 * 补弹本身由服务器完成（Auth_CommitReload），不依赖客户端动画结束
 */
UCLASS()
class FPS_API UGA_Reload : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UGA_Reload();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData)override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled)override;

	//超时兜底：超过这个时间还没收到换弹结束就强制结束，防止 State.Reloading 残留
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Reload")
	float ReloadTimeout;

private:

	void OnReloadFinished();
	void OnReloadTimeout();

	static UCombatComponent* GetCombat(const FGameplayAbilityActorInfo* ActorInfo);

	FDelegateHandle ReloadFinishedHandle;
	FTimerHandle TimeoutHandle;
	TWeakObjectPtr<UCombatComponent> BoundCombat;
};

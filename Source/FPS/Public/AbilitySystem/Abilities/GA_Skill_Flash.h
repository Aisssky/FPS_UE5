// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Skill_Flash.generated.h"

/*
 * 闪光技能（S3-4）：ServerOnly，服务器权威
 * - CommitAbility 原子检查 Cooldown（Cooldown.Skill.Flash）
 * - 服务器遍历其它角色，按「目标是否面向闪光点 + 视线是否被遮挡」判定致盲
 * - 被致盲者挂 State.Blinded 标签（复制），开火被 ActivationBlockedTags 挡掉
 * 落点仍用角色瞄准方向直线投射（灰盒阶段同烟雾）
 */
UCLASS()
class FPS_API UGA_Skill_Flash : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Skill_Flash();

	//S4-4：购买价格（Buy 阶段花 Money 解锁，服务器读）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Skill|Economy")
	int32 BuyPrice;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	//冷却时长（秒）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Skill|Flash")
	float CooldownSeconds;

	//致盲持续时长（秒）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Skill|Flash")
	float BlindDuration;

	//闪光最远作用半径
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Skill|Flash")
	float FlashRadius;

	//投掷最远距离
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Skill|Flash")
	float MaxThrowDistance;

	//视线夹角阈值（度）：目标面向闪光点与视线方向的夹角小于它才致盲（背对不受影响）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Skill|Flash")
	float FacingAngleThreshold;

	//服务器：投射落点、遍历目标、判定并施加致盲
	void ResolveFlashOnServer(const FGameplayAbilityActorInfo* ActorInfo);

	//对单个目标：返回是否被致盲（面向 + 无遮挡）
	bool IsTargetBlinded(AActor* Target, AActor* Instigator, const FVector& FlashLocation) const;

	//S3-8：读双方 PlayerState.TeamID 判断同队。TeamID 任一为 0（未分配）时返回 false（退回只影响非 Owner 规则）
	bool IsSameTeam(AActor* A, AActor* B) const;
};

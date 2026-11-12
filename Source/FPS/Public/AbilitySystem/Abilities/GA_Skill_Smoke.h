// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Skill_Smoke.generated.h"

class ASkillSmokeActor;

/*
 * 烟雾技能（S3-4）：ServerOnly，服务器权威
 * - CommitAbility 原子检查 Cost + Cooldown（Cooldown GE 挂 Cooldown.Skill.Smoke）
 * - 落点 = 角色瞄准方向直线投射（灰盒阶段不引入 TargetActor 抛物线），服务器自算、客户端不上传命中
 * - 生成 ASkillSmokeActor（复制），表现走 GameplayCue.Skill.Smoke
 * 与 GA_FireWeapon 的薄壳定位不同：技能本身就是权威逻辑，所以在这里做裁决，不拆到 CombatComponent
 */
UCLASS()
class FPS_API UGA_Skill_Smoke : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Skill_Smoke();

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
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Skill|Smoke")
	float CooldownSeconds;

	//烟雾持续时长（秒）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Skill|Smoke")
	float SmokeLifetime;

	//烟雾半径
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Skill|Smoke")
	float SmokeRadius;

	//投掷最远距离
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Skill|Smoke")
	float MaxThrowDistance;

	//服务器：从角色瞄准方向投射落点并生成烟雾
	void SpawnSmokeOnServer(const FGameplayAbilityActorInfo* ActorInfo);

	//在指定位置 Spawn 烟雾 Actor + 触发烟雾 Cue
	void SpawnSmokeActor(const FVector& Location);
};

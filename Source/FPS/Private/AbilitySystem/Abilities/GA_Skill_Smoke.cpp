// Copyright Aisssky


#include "AbilitySystem/Abilities/GA_Skill_Smoke.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Effects/GE_Cooldown_Smoke.h"
#include "AbilitySystem/Skills/SkillSmokeActor.h"
#include "Character/ShooterCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Tags/ShooterGameplayTags.h"

UGA_Skill_Smoke::UGA_Skill_Smoke()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	//技能是权威逻辑：只在服务器执行，客户端只通过复制看到结果
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	SetAssetTags(FGameplayTagContainer(ShooterTags::TAG_Ability_Skill_Smoke));
	//死亡时不能放技能
	ActivationBlockedTags.AddTag(ShooterTags::TAG_State_Dead);

	//默认值（蓝图可改）
	BuyPrice = 200;
	CooldownSeconds = 20.f;
	SmokeLifetime = 8.f;
	SmokeRadius = 300.f;
	MaxThrowDistance = 1500.f;

	//冷却用 GE：提交后挂 Cooldown.Skill.Smoke 标签
	CooldownGameplayEffectClass = UGE_Cooldown_Smoke::StaticClass();
}

void UGA_Skill_Smoke::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	/*
	 * CommitAbility：一次性原子检查 Cost + Cooldown
	 * - 检查 CooldownGameplayEffectClass 对应的标签（Cooldown.Skill.Smoke）是否还在
	 * - 若有 Cost GE 也一并检查，通过后应用 Cooldown GE
	 * 冷却中/资源不足时返回 false，这里直接 EndAbility（服务器拒绝）
	 */
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SpawnSmokeOnServer(ActorInfo);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_Skill_Smoke::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Skill_Smoke::SpawnSmokeOnServer(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid()) return;

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	if (!World) return;

	//落点：角色瞄准方向直线投射（灰盒阶段不引入投掷物抛物线）
	FVector Start = Avatar->GetActorLocation();
	FVector AimDir = FVector::ZeroVector;

	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		Start = Character->GetPawnViewLocation();
		AimDir = Character->GetBaseAimRotation().Vector();
	}
	else
	{
		AimDir = Avatar->GetActorForwardVector();
	}

	const FVector End = Start + AimDir * MaxThrowDistance;

	//服务器 trace 找落点（打到地面/墙就落在命中点，否则落最远点）
	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams))
	{
		//命中点沿法线回退一点，避免烟球体一半埋进墙里
		const FVector SpawnLoc = Hit.ImpactPoint + Hit.ImpactNormal * 10.f;
		SpawnSmokeActor(SpawnLoc);
	}
	else
	{
		SpawnSmokeActor(End);
	}
}

void UGA_Skill_Smoke::SpawnSmokeActor(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetAvatarActorFromActorInfo();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASkillSmokeActor* Smoke = World->SpawnActor<ASkillSmokeActor>(ASkillSmokeActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
	if (Smoke)
	{
		Smoke->ServerInitSmoke(Location, SmokeRadius, SmokeLifetime);

		//表现走 Cue：把烟雾位置/半径塞进 Cue 参数（灰盒阶段 Cue 只做信号 + 日志，表现蓝图侧接）
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC)
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Location;
			CueParams.RawMagnitude = SmokeRadius;
			ASC->ExecuteGameplayCue(ShooterTags::TAG_Cue_Skill_Smoke, CueParams);
		}
	}
}

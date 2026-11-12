// Copyright Aisssky


#include "AbilitySystem/Abilities/GA_Skill_Flash.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Effects/GE_Cooldown_Flash.h"
#include "Character/ShooterCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/ShooterGameplayTags.h"

UGA_Skill_Flash::UGA_Skill_Flash()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	SetAssetTags(FGameplayTagContainer(ShooterTags::TAG_Ability_Skill_Flash));
	ActivationBlockedTags.AddTag(ShooterTags::TAG_State_Dead);

	BuyPrice = 250;
	CooldownSeconds = 30.f;
	BlindDuration = 4.f;
	FlashRadius = 1200.f;
	MaxThrowDistance = 1500.f;
	FacingAngleThreshold = 60.f;

	CooldownGameplayEffectClass = UGE_Cooldown_Flash::StaticClass();
}

void UGA_Skill_Flash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	//原子检查冷却：冷却标签还在就拒绝
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ResolveFlashOnServer(ActorInfo);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_Skill_Flash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Skill_Flash::ResolveFlashOnServer(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid()) return;

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	if (!World) return;

	//落点：瞄准方向直线投射
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
	FVector FlashLocation = End;

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams))
	{
		FlashLocation = Hit.ImpactPoint;
	}

	//遍历所有角色，判定致盲
	for (TActorIterator<AShooterCharacter> It(World); It; ++It)
	{
		AShooterCharacter* Target = *It;
		if (!IsValid(Target) || Target == Avatar) continue;

		//S3-8 队伍判定：队友不受闪光影响。TeamID==0（未分配）退回「只影响非 Owner」的旧规则
		if (IsSameTeam(Avatar, Target)) continue;

		//距离过滤：超出闪光半径直接跳过
		if (FVector::Dist(Target->GetActorLocation(), FlashLocation) > FlashRadius) continue;

		if (IsTargetBlinded(Target, Avatar, FlashLocation))
		{
			//致盲：给目标 ASC 挂复制的 State.Blinded 标签，持续 BlindDuration
			if (UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent())
			{
				TargetASC->AddLooseGameplayTag(ShooterTags::TAG_State_Blinded, 1, EGameplayTagReplicationState::TagOnly);

				//延迟移除：服务器计时器到期摘标签
				FTimerHandle BlindEndHandle;
				FTimerDelegate BlindEndDelegate;
				TWeakObjectPtr<UAbilitySystemComponent> WeakASC = TargetASC;
				BlindEndDelegate.BindLambda([WeakASC]()
				{
					if (WeakASC.IsValid())
					{
						WeakASC->SetLooseGameplayTagCount(ShooterTags::TAG_State_Blinded, 0, EGameplayTagReplicationState::TagOnly);
					}
				});
				World->GetTimerManager().SetTimer(BlindEndHandle, BlindEndDelegate, BlindDuration, false);
			}
		}
	}

	//表现 Cue
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = FlashLocation;
		CueParams.RawMagnitude = FlashRadius;
		ASC->ExecuteGameplayCue(ShooterTags::TAG_Cue_Skill_Flash, CueParams);
	}
}

bool UGA_Skill_Flash::IsTargetBlinded(AActor* Target, AActor* Instigator, const FVector& FlashLocation) const
{
	if (!IsValid(Target)) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	//目标面向判定：目标视线方向 vs 目标到闪光点方向，夹角小于阈值才算「正对闪光」
	const FVector ToFlash = (FlashLocation - Target->GetActorLocation()).GetSafeNormal();
	FVector TargetViewDir = FVector::ZeroVector;
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		TargetViewDir = Character->GetBaseAimRotation().Vector();
	}
	else
	{
		TargetViewDir = Target->GetActorForwardVector();
	}

	const float FacingDot = FVector::DotProduct(TargetViewDir, ToFlash);
	const float FacingAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FacingDot, -1.f, 1.f)));
	if (FacingAngle > FacingAngleThreshold) return false;

	//视线遮挡判定：闪光点到目标眼睛之间有没有墙挡着（有遮挡 = 没被闪到）
	const FVector TargetEye = Target->GetActorLocation() + FVector(0.f, 0.f, 50.f); // 近似眼睛位置
	FHitResult BlockHit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Target);
	QueryParams.AddIgnoredActor(Instigator);
	if (World->LineTraceSingleByChannel(BlockHit, FlashLocation, TargetEye, ECC_Visibility, QueryParams))
	{
		return false; // 有遮挡，不闪
	}

	return true;
}

bool UGA_Skill_Flash::IsSameTeam(AActor* A, AActor* B) const
{
	if (!IsValid(A) || !IsValid(B)) return false;

	//经 Pawn 拿 PlayerState，读 TeamID。任一拿不到或 TeamID==0（未分配）都视为「无法判定同队」
	const APawn* PawnA = Cast<APawn>(A);
	const APawn* PawnB = Cast<APawn>(B);
	if (!PawnA || !PawnB) return false;

	const AShooterPlayerState* PSA = Cast<AShooterPlayerState>(PawnA->GetPlayerState());
	const AShooterPlayerState* PSB = Cast<AShooterPlayerState>(PawnB->GetPlayerState());
	if (!PSA || !PSB) return false;

	if (PSA->TeamID == 0 || PSB->TeamID == 0) return false; // 未分配队伍，退回「只影响非 Owner」

	return PSA->TeamID == PSB->TeamID;
}

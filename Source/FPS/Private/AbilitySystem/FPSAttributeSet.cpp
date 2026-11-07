// Copyright Aisssky


#include "AbilitySystem/FPSAttributeSet.h"
#include "AbilitySystem/FPSAbilitySystemComponent.h"
#include "Character/ShooterCharacter.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Tags/ShooterGameplayTags.h"

UFPSAttributeSet::UFPSAttributeSet()
{
	//默认值；每次出生由 AShooterCharacter::InitializeAttributesForSpawn 按蓝图配置重置
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
}

void UFPSAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UFPSAttributeSet,Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFPSAttributeSet,MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFPSAttributeSet,Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFPSAttributeSet,MaxStamina, COND_None, REPNOTIFY_Always);


}

void UFPSAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFPSAttributeSet,Health,OldHealth);
}

void UFPSAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFPSAttributeSet,MaxHealth,OldMaxHealth);
}

void UFPSAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFPSAttributeSet,Stamina,OldStamina);
}

void UFPSAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFPSAttributeSet,MaxStamina,OldMaxStamina);
}

void UFPSAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute==GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(),0.f,GetMaxHealth()));

		//把结果同步给角色：HealthComponent
		AShooterCharacter* Avatar = Cast<AShooterCharacter>(Data.Target.GetAvatarActor());
		AActor* DamageInstigator = Data.EffectSpec.GetContext().GetInstigator();
		if (Avatar)
		{
			Avatar->HandleHealthChanged(GetHealth(), GetMaxHealth(), Data.EvaluatedData.Magnitude, DamageInstigator);
		}

		HandleDeathIfNeeded(Data);
	}

	if (Data.EvaluatedData.Attribute==GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(GetMaxHealth(),0.f));
		SetHealth(FMath::Clamp(GetHealth(),0.f,GetMaxHealth()));

	}

	if (Data.EvaluatedData.Attribute==GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(),0.f,GetMaxStamina()));

	}

	if (Data.EvaluatedData.Attribute==GetMaxStaminaAttribute())
	{
		SetMaxStamina(FMath::Max(GetMaxStamina(),0.f));
		SetStamina(FMath::Clamp(GetStamina(),0.f,GetMaxStamina()));

	}
}

void UFPSAttributeSet::HandleDeathIfNeeded(const FGameplayEffectModCallbackData& Data)
{
	if (GetHealth() > 0.f) return;

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC) return;

	//死亡
	if (ASC->HasMatchingGameplayTag(ShooterTags::TAG_State_Dead)) return;
	
	ASC->AddLooseGameplayTag(ShooterTags::TAG_State_Dead, 1, EGameplayTagReplicationState::TagOnly);

	//驱动原有死亡流程（DeathState 复制、OnDeathStarted：销毁武器/重生计时/关碰撞）
	if (AShooterCharacter* Avatar = Cast<AShooterCharacter>(Data.Target.GetAvatarActor()))
	{
		Avatar->HandleOutOfHealth(Data.EffectSpec.GetContext().GetInstigator());
	}
}

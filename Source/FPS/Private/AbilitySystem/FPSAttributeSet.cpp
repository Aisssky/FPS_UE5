// Copyright Aisssky


#include "AbilitySystem/FPSAttributeSet.h"
#include "AbilitySystem/FPSAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

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
		
	}
	
	if (Data.EvaluatedData.Attribute==GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Clamp(GetMaxHealth(),0.f,GetMaxHealth()));
		
	}
	
	if (Data.EvaluatedData.Attribute==GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(),0.f,GetMaxStamina()));
		
	}
	
	if (Data.EvaluatedData.Attribute==GetMaxStaminaAttribute())
	{
		SetMaxStamina(FMath::Clamp(GetMaxStamina(),0.f,GetMaxStamina()));
		
	}
}

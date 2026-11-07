// Copyright Aisssky


#include "AbilitySystem/Effects/GE_Damage.h"

#include "AbilitySystem/FPSAttributeSet.h"
#include "Tags/ShooterGameplayTags.h"

UGE_Damage::UGE_Damage()
{
	//瞬时效果：直接改 Base 值，不留 Active Effect
	DurationPolicy = EGameplayEffectDurationType::Instant;
	
	//Health += SetByCaller(Data.Damage)；调用方传负数表示扣血
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ShooterTags::TAG_Data_Damage;
	
	FGameplayModifierInfo HealthModifier;
	HealthModifier.Attribute = UFPSAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::AddBase;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	
	Modifiers.Add(HealthModifier);
}

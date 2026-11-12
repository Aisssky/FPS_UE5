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

	/*
	 * S1-8：伤害与受击表现同源，通过 GE 的 GameplayCue 数组触发 GC_Damage
	 * GE 在服务器应用时，目标 ASC 会自动按这里的 Tag 触发 Cue（客户端收到复制后播表现）
	 * 不靠客户端自 trace，被拒绝的射击根本不会产生这个 GE，也就不会触发受击 Cue
	 */
	FGameplayEffectCue DamageCue;
	DamageCue.GameplayCueTags.AddTag(ShooterTags::TAG_Cue_Damage);
	GameplayCues.Add(DamageCue);
}

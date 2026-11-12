// Copyright Aisssky


#include "AbilitySystem/Effects/GE_Cooldown_Flash.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/ShooterGameplayTags.h"

UGE_Cooldown_Flash::UGE_Cooldown_Flash()
{
	//持续效果：到期标签自动移除
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	//时长由 GA 在 Spec 里用 SetByCaller(Data.Cooldown.Flash) 写入
	FSetByCallerFloat CooldownSetter;
	CooldownSetter.DataTag = ShooterTags::TAG_Data_Cooldown_Flash;
	DurationMagnitude = FGameplayEffectModifierMagnitude(CooldownSetter);

	//作用期间给角色挂冷却标签，CommitAbility 靠它挡重复释放
	//注意：FindOrAddComponent 内部 NewObject(NAME_None)，不能用在构造函数里（默认子对象创建阶段会崩）
	UTargetTagsGameplayEffectComponent* TargetTagsComp = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ShooterTags::TAG_Cooldown_Skill_Flash);
	TargetTagsComp->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComp);
}

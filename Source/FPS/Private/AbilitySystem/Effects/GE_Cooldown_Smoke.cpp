// Copyright Aisssky


#include "AbilitySystem/Effects/GE_Cooldown_Smoke.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/ShooterGameplayTags.h"

UGE_Cooldown_Smoke::UGE_Cooldown_Smoke()
{
	//持续效果：到期标签自动移除
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	//时长由 GA 在 Spec 里用 SetByCaller(Data.Cooldown.Smoke) 写入
	FSetByCallerFloat CooldownSetter;
	CooldownSetter.DataTag = ShooterTags::TAG_Data_Cooldown_Smoke;
	DurationMagnitude = FGameplayEffectModifierMagnitude(CooldownSetter);

	//作用期间给角色挂冷却标签，CommitAbility 靠它挡重复释放（UE5.8 用 TargetTags 组件，不再用弃用的 InheritableOwnedTagsContainer）
	//注意：FindOrAddComponent 内部 NewObject(NAME_None)，不能用在构造函数里（默认子对象创建阶段会崩）；
	//这里用 CreateDefaultSubobject 显式创建组件，再塞进 GEComponents 数组
	UTargetTagsGameplayEffectComponent* TargetTagsComp = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ShooterTags::TAG_Cooldown_Skill_Smoke);
	TargetTagsComp->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComp);
}

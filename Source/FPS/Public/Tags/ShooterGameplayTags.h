
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace ShooterTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_WeaponType_None);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_WeaponType_Rifle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_WeaponType_Pistol);
	
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Firing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Reloading);
	//死亡标记（复制的 Loose Tag），阻止一切 Ability，防止死亡重复触发
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dead);
	
	//Ability 自身的标签（AssetTags），用于按标签激活/取消
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Reload);
	//技能 Ability（S3）：AssetTags + 按标签激活
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Skill_Smoke);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Skill_Flash);
	
	//状态：被闪光致盲（复制的 Loose Tag，客户端据此屏蔽开火 + 刷白屏）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Blinded);
	
	//冷却标签：技能释放后给角色挂 Cooldown.*，CommitAbility 用它挡重复释放
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Skill_Smoke);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Skill_Flash);
	
	//SetByCaller 数据标签：伤害值由服务器算好后塞进 GE Spec
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage);
	//技能冷却时长（SetByCaller，服务器写进 Cooldown GE 的 Duration）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Cooldown_Smoke);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Cooldown_Flash);

	//GameplayCue 标签：表现层（枪口焰/弹着/受击）统一走 Cue 通道，前缀必须 GameplayCue.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cue_WeaponFire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cue_Impact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cue_Damage);
	//技能 Cue（S3）：烟/闪表现走同一套 Cue 通道
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cue_Skill_Smoke);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cue_Skill_Flash);
}

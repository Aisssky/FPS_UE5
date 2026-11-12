#include "Tags/ShooterGameplayTags.h"

namespace ShooterTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_WeaponType_None, "Weapon.Type.None", "No Weapon Type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_WeaponType_Rifle, "Weapon.Type.Rifle", "Rifle Weapon Type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_WeaponType_Pistol, "Weapon.Type.Pistol", "Pistol Weapon Type");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Firing, "State.Firing", "State Fire");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Reloading, "State.Reloading", "State Reload");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Dead, "State.Dead", "State Dead");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Fire, "Ability.Fire", "Fire Weapon Ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Reload, "Ability.Reload", "Reload Weapon Ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Skill_Smoke, "Ability.Skill.Smoke", "烟雾技能");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Ability_Skill_Flash, "Ability.Skill.Flash", "闪光技能");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_State_Blinded, "State.Blinded", "被闪光致盲");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cooldown_Skill_Smoke, "Cooldown.Skill.Smoke", "烟雾冷却中");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cooldown_Skill_Flash, "Cooldown.Skill.Flash", "闪光冷却中");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Damage, "Data.Damage", "SetByCaller Damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Cooldown_Smoke, "Data.Cooldown.Smoke", "烟雾冷却时长");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Cooldown_Flash, "Data.Cooldown.Flash", "闪光冷却时长");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cue_WeaponFire, "GameplayCue.WeaponFire", "开火表现（枪口焰/枪声）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cue_Impact, "GameplayCue.Impact", "弹着表现（弹孔/火花）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cue_Damage, "GameplayCue.Damage", "受击/死亡反馈");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cue_Skill_Smoke, "GameplayCue.Skill.Smoke", "烟雾表现");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Cue_Skill_Flash, "GameplayCue.Skill.Flash", "闪光表现");
}

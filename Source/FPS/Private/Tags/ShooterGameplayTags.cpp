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
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Damage, "Data.Damage", "SetByCaller Damage");
}

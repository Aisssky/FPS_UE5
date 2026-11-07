
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
	
	//SetByCaller 数据标签：伤害值由服务器算好后塞进 GE Spec
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage);
}

// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsBodyInstanceOwnerInterface.h"

//碰撞通道 比宏更有类型安全性
namespace FPSTraceChannels
{
	constexpr ECollisionChannel ECC_Weapon = ECC_GameTraceChannel1;
}

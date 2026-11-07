// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Damage.generated.h"

/*
 * 伤害 GE（M7）：Instant，修改 Health，数值走 SetByCaller(Data.Damage)
 * 伤害值由服务器（HitValidationComponent）按武器配置/命中部位算好再塞进来，客户端碰不到
 * 用 C++ 写死 Modifier，避免蓝图资产漏配；需要加 GameplayCue 等表现时可以再派生蓝图子类
 */
UCLASS()
class FPS_API UGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Damage();
};

// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cooldown_Flash.generated.h"

/*
 * 闪光冷却 GE（S3-2）：HasDuration，挂 Cooldown.Skill.Flash 标签
 * 时长走 SetByCaller(Data.Cooldown.Flash)，由 GA_Skill_Flash 在 Commit 时写入
 */
UCLASS()
class FPS_API UGE_Cooldown_Flash : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Cooldown_Flash();
};

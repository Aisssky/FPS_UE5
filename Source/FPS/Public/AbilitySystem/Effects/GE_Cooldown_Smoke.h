// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cooldown_Smoke.generated.h"

/*
 * 烟雾冷却 GE（S3-2）：HasDuration，挂 Cooldown.Skill.Smoke 标签
 * 时长走 SetByCaller(Data.Cooldown.Smoke)，由 GA_Skill_Smoke 在 Commit 时写入
 * CommitAbility 原子检查：冷却标签还在（Cooldown 未到期）就拒绝激活
 */
UCLASS()
class FPS_API UGE_Cooldown_Smoke : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Cooldown_Smoke();
};

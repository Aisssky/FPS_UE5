// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "HistoricalHitboxTypes.generated.h"

/*
 * 历史 hitbox 数据（M5）
 * 只在服务器内存里用，不复制。用 USTRUCT 是为了编辑器里配置 hitbox 定义 + 调试方便，
 * 不是习惯性全加 UPROPERTY。
 */

//hitbox 定义：在角色蓝图里配置「哪根骨骼挂多大的盒子」
USTRUCT(BlueprintType)
struct FHitboxDefinition
{
	GENERATED_BODY()
	
	//逻辑名（head/torso/pelvis），命中结果里返回它；head 判定爆头
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Hitbox")
	FName HitboxName=NAME_None;
	
	//挂在哪根骨骼/插槽上
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Hitbox")
	FName BoneName=NAME_None;
	
	//盒子半尺寸（骨骼空间）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Hitbox")
	FVector BoxExtent=FVector(10.f);
	
	//相对骨骼的偏移（骨骼空间）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Hitbox")
	FVector LocalOffset=FVector::ZeroVector;
};

//某一时刻的一个 hitbox
USTRUCT()
struct FHistoricalHitbox
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName HitboxName=NAME_None;
	
	//世界空间（不含缩放）
	UPROPERTY()
	FTransform WorldTransform;
	
	UPROPERTY()
	FVector BoxExtent=FVector::ZeroVector;
};

//某一时刻的一整帧
USTRUCT()
struct FHistoricalFrame
{
	GENERATED_BODY()
	
	//服务器世界时间（秒），与 UNetworkTimeSubsystem 同一时间轴
	UPROPERTY()
	double ServerTime=0.0;
	
	UPROPERTY()
	FTransform ActorTransform;
	
	UPROPERTY()
	TArray<FHistoricalHitbox> Hitboxes;
};

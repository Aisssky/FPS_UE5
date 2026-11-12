// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "ShotRequest.generated.h"

USTRUCT()
struct FShotRequest
{
	GENERATED_BODY()
	
	//服务器去重
	UPROPERTY()
	uint32 ShotSequence=0;
	
	UPROPERTY()
	double ClientShotTime=0;
	
	UPROPERTY()
	FVector_NetQuantize TraceOrigin;
	
	UPROPERTY()
	FVector_NetQuantizeNormal AimDirection;

	/*
	 * 散布种子（S2-4）：客户端按此种子做本地预测散布，服务器用同一种子在权威侧复现
	 * 客户端只带「射击意图 + 方向 + 种子」，最终命中方向由服务器施加散布后自算
	 * 0 = 无散布（单机/未配置散布数据时走旧路径，服务器不扰动）
	 */
	UPROPERTY()
	int32 SpreadSeed=0;
};
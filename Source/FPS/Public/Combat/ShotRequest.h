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
};
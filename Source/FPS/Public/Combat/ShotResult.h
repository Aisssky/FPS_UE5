// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Networking/NetworkValidationTypes.h"
#include "ShotResult.generated.h"

/*
 * 服务器 -> 客户端/服务器内部：一次射击的权威结果
 * 与 FShotRequest（客户端 -> 服务器）职责相反，不要混用
 * 这里的所有命中数据都是服务器自己 trace 算出来的，不是客户端输入
 */
USTRUCT()
struct FShotResult
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint32 ShotSequence=0;
	
	UPROPERTY()
	bool bAccepted=false;
	
	//结构化拒绝原因，见 §17.2
	UPROPERTY()
	EShotRejectReason RejectReason=EShotRejectReason::None;
	
	//校验/换算后的服务器射击时刻（回溯用）
	UPROPERTY()
	double ValidatedServerShotTime=0.0;
	
	//服务器自算命中的 Actor（仅服务器内部结果）
	UPROPERTY()
	TObjectPtr<AActor> HitActor=nullptr;
	
	UPROPERTY()
	FName HitboxName=NAME_None;
	
	UPROPERTY()
	FVector_NetQuantize ImpactPoint;
	
	UPROPERTY()
	FVector_NetQuantizeNormal ImpactNormal;
	
	//命中表面类型，给枪口/弹孔特效用（无物理材质时沿用旧代码的 SurfaceType1）
	UPROPERTY()
	TEnumAsByte<EPhysicalSurface> SurfaceType=SurfaceType1;
	
	//服务器算出的伤害，仅用于表现回显
	UPROPERTY()
	float Damage=0.f;
	
	UPROPERTY()
	bool bHeadShot=false;
	
	//由 GAS 死亡判定得出（不是 DoDamage 返回值）
	UPROPERTY()
	bool bLethal=false;
	
	//true=走了历史 hitbox 回溯；false=降级为当前世界 trace
	UPROPERTY()
	bool bUsedRewind=false;
	
	bool HasHit() const { return HitActor!=nullptr; }
};

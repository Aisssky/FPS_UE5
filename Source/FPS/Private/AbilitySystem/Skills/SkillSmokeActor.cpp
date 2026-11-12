// Copyright Aisssky


#include "AbilitySystem/Skills/SkillSmokeActor.h"

#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

ASkillSmokeActor::ASkillSmokeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	//灰盒视觉：引擎自带球体占位，Scale 在 ServerInitSmoke 里按 SmokeRadius 缩放（Sphere 默认半径 50）
	SmokeVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SmokeVisual"));
	RootComponent = SmokeVisual;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		SmokeVisual->SetStaticMesh(SphereMesh.Object);
	}
	SmokeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SmokeVisual->SetCastShadow(false);

	SmokeLocation = FVector::ZeroVector;
	SmokeRadius = 300.f;
	SpawnServerTime = 0.0;
	ExpireServerTime = 0.0;
}

void ASkillSmokeActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASkillSmokeActor, SmokeLocation);
	DOREPLIFETIME(ASkillSmokeActor, SmokeRadius);
}

void ASkillSmokeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASkillSmokeActor::ServerInitSmoke(const FVector& InLocation, float InRadius, float InLifetime)
{
	if (!HasAuthority()) return;

	SmokeLocation = InLocation;
	SmokeRadius = InRadius;

	const UWorld* World = GetWorld();
	SpawnServerTime = World ? World->GetRealTimeSeconds() : 0.0;
	ExpireServerTime = SpawnServerTime + InLifetime;

	SetActorLocation(InLocation);

	//灰盒视觉：球体半径跟随 SmokeRadius（Sphere 默认半径 50）
	SetActorScale3D(FVector(InRadius / 50.f));

	//到期销毁：服务器计时器，销毁会经复制同步到客户端
	if (World && InLifetime > 0.f)
	{
		World->GetTimerManager().SetTimer(LifetimeHandle, this, &ASkillSmokeActor::OnLifetimeExpired, InLifetime, false);
	}
}

bool ASkillSmokeActor::IsActiveAtTime(double ServerTime) const
{
	//服务器上：用精确的生成/到期时刻；客户端上（复制延迟）退回用当前是否还活着
	if (HasAuthority())
	{
		return ServerTime >= SpawnServerTime && ServerTime < ExpireServerTime;
	}
	return !IsPendingKillPending();
}

bool ASkillSmokeActor::BlocksLine(const FVector& Start, const FVector& Dir, float MaxDistance, float& OutDistance) const
{
	//射线 vs 球体：解一元二次方程，取进入球体（较近）的交点
	const FVector L = SmokeLocation - Start;
	const float DirLen = Dir.Size();
	if (DirLen < KINDA_SMALL_NUMBER) return false;

	const FVector D = Dir / DirLen;
	const float TCA = FVector::DotProduct(L, D);
	if (TCA < 0.f) return false; // 球心在起点后方

	const float D2 = FVector::DotProduct(L, L) - TCA * TCA;
	const float R2 = SmokeRadius * SmokeRadius;
	if (D2 > R2) return false; // 射线不碰球体

	//进入球体的距离
	const float Thc = FMath::Sqrt(FMath::Max(0.f, R2 - D2));
	const float TEnter = TCA - Thc;

	//起点在球体内时 TEnter < 0，视为从起点就被遮挡
	const float ClampedEnter = FMath::Max(0.f, TEnter);
	if (ClampedEnter > MaxDistance) return false;

	OutDistance = ClampedEnter;
	return true;
}

void ASkillSmokeActor::OnLifetimeExpired()
{
	Destroy();
}

// Copyright Aisssky


#include "Networking/LagCompensationComponent.h"

#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"


ULagCompensationComponent::ULagCompensationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	//动画/移动都更新完再记录，保证记的是这一帧最终的 hitbox 位置
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	//历史只在服务器内存里，不需要复制
	SetIsReplicatedByDefault(false);

	MaxRecordTimeSeconds = 1.f;

	//默认骨骼名按 UE 小白人；换骨架时在 BP_ShooterCharacter 里改
	FHitboxDefinition Head;
	Head.HitboxName = "head";
	Head.BoneName = "head";
	Head.BoxExtent = FVector(12.f);
	HitboxDefinitions.Add(Head);

	FHitboxDefinition Torso;
	Torso.HitboxName = "torso";
	Torso.BoneName = "spine_03";
	Torso.BoxExtent = FVector(22.f);
	HitboxDefinitions.Add(Torso);

	FHitboxDefinition Pelvis;
	Pelvis.HitboxName = "pelvis";
	Pelvis.BoneName = "pelvis";
	Pelvis.BoxExtent = FVector(18.f);
	HitboxDefinitions.Add(Pelvis);
}

void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();

	//只在服务器记录；客户端直接关掉 Tick，省 CPU
	if (!ShouldRecord())
	{
		SetComponentTickEnabled(false);
		return;
	}

	//验证 hitbox 配置：骨骼不存在的直接报出来，别等到命中判定才发现
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LagComp] %s has no skeletal mesh, history disabled"), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}
	for (const FHitboxDefinition& Def : HitboxDefinitions)
	{
		if (!Mesh->DoesSocketExist(Def.BoneName))
		{
			UE_LOG(LogTemp, Warning, TEXT("[LagComp] %s hitbox %s: bone %s not found"), *GetNameSafe(GetOwner()), *Def.HitboxName.ToString(), *Def.BoneName.ToString());
		}
	}

	//预留容量：按 60Hz 估算，避免运行时反复扩容
	History.Reserve(FMath::CeilToInt(MaxRecordTimeSeconds * 60.f) + 2);
}

bool ULagCompensationComponent::ShouldRecord() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority() && GetNetMode() != NM_Client;
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RecordHistoricalFrame();
	TrimHistory();
}

void ULagCompensationComponent::BuildFrameNow(FHistoricalFrame& OutFrame) const
{
	OutFrame.ServerTime = GetWorld()->GetRealTimeSeconds();
	OutFrame.ActorTransform = GetOwner()->GetActorTransform();
	OutFrame.Hitboxes.Reset(HitboxDefinitions.Num());

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh) return;

	for (const FHitboxDefinition& Def : HitboxDefinitions)
	{
		//骨骼世界变换，去掉缩放（盒子尺寸由 BoxExtent 决定）
		FTransform BoneTransform = Mesh->GetSocketTransform(Def.BoneName, RTS_World);
		BoneTransform.SetScale3D(FVector::OneVector);

		FHistoricalHitbox& Box = OutFrame.Hitboxes.AddDefaulted_GetRef();
		Box.HitboxName = Def.HitboxName;
		Box.BoxExtent = Def.BoxExtent;
		Box.WorldTransform = FTransform(Def.LocalOffset) * BoneTransform;
	}
}

void ULagCompensationComponent::RecordHistoricalFrame()
{
	if (!ShouldRecord()) return;

	FHistoricalFrame Frame;
	BuildFrameNow(Frame);

	//同一时刻重复记录（一帧 Tick 两次）就覆盖
	if (History.Num() > 0 && FMath::IsNearlyEqual(History.Last().ServerTime, Frame.ServerTime))
	{
		History.Last() = MoveTemp(Frame);
		return;
	}
	History.Add(MoveTemp(Frame));
}

void ULagCompensationComponent::TrimHistory()
{
	if (History.Num() == 0) return;

	const double OldestAllowed = GetWorld()->GetRealTimeSeconds() - MaxRecordTimeSeconds;

	//至少保留一帧在窗口外，保证窗口边界的时刻仍能插值
	int32 RemoveCount = 0;
	while (RemoveCount + 1 < History.Num() && History[RemoveCount + 1].ServerTime < OldestAllowed)
	{
		++RemoveCount;
	}
	if (RemoveCount > 0)
	{
		History.RemoveAt(0, RemoveCount, EAllowShrinking::No);
	}
}

void ULagCompensationComponent::ClearHistory()
{
	History.Reset();
}

bool ULagCompensationComponent::GetHistoricalFrameAtTime(double ServerTime, FHistoricalFrame& OutFrame) const
{
	if (History.Num() == 0) return false;

	//早于最老帧：没有数据，明确失败（目标可能刚出生）
	if (ServerTime < History[0].ServerTime) return false;

	//晚于最新帧：最多差一个 Tick，直接用最新帧
	if (ServerTime >= History.Last().ServerTime)
	{
		OutFrame = History.Last();
		return true;
	}

	return InterpolateHistoricalFrame(ServerTime, OutFrame);
}

bool ULagCompensationComponent::InterpolateHistoricalFrame(double ServerTime, FHistoricalFrame& OutFrame) const
{
	//找 TimeBefore <= T <= TimeAfter 的两帧（数组按时间有序，从新往旧找更快命中常见的小回溯）
	for (int32 i = History.Num() - 1; i > 0; --i)
	{
		const FHistoricalFrame& Newer = History[i];
		const FHistoricalFrame& Older = History[i - 1];
		if (Older.ServerTime <= ServerTime && ServerTime <= Newer.ServerTime)
		{
			//完全匹配直接返回
			if (FMath::IsNearlyEqual(Older.ServerTime, ServerTime))
			{
				OutFrame = Older;
				return true;
			}
			if (FMath::IsNearlyEqual(Newer.ServerTime, ServerTime))
			{
				OutFrame = Newer;
				return true;
			}

			//hitbox 配置对不上就失败，不拼接错误数据
			if (Older.Hitboxes.Num() != Newer.Hitboxes.Num()) return false;

			const double Alpha = (ServerTime - Older.ServerTime) / (Newer.ServerTime - Older.ServerTime);
			LerpFrame(Older, Newer, Alpha, OutFrame);
			return OutFrame.Hitboxes.Num() == Older.Hitboxes.Num();
		}
	}
	return false;
}

void ULagCompensationComponent::LerpFrame(const FHistoricalFrame& Older, const FHistoricalFrame& Newer, double Alpha, FHistoricalFrame& OutFrame)
{
	OutFrame.ServerTime = FMath::Lerp(Older.ServerTime, Newer.ServerTime, Alpha);
	OutFrame.ActorTransform.Blend(Older.ActorTransform, Newer.ActorTransform, Alpha);
	OutFrame.Hitboxes.Reset(Older.Hitboxes.Num());

	for (int32 i = 0; i < Older.Hitboxes.Num(); ++i)
	{
		const FHistoricalHitbox& A = Older.Hitboxes[i];
		const FHistoricalHitbox& B = Newer.Hitboxes[i];
		if (A.HitboxName != B.HitboxName)
		{
			//名字对不上：返回空数组让调用方判失败
			OutFrame.Hitboxes.Reset();
			return;
		}

		FHistoricalHitbox& Box = OutFrame.Hitboxes.AddDefaulted_GetRef();
		Box.HitboxName = A.HitboxName;
		Box.BoxExtent = FMath::Lerp(A.BoxExtent, B.BoxExtent, Alpha);
		//位置 Lerp，旋转用四元数 Slerp（不要对欧拉角插值）
		Box.WorldTransform.SetLocation(FMath::Lerp(A.WorldTransform.GetLocation(), B.WorldTransform.GetLocation(), Alpha));
		Box.WorldTransform.SetRotation(FQuat::Slerp(A.WorldTransform.GetRotation(), B.WorldTransform.GetRotation(), Alpha));
		Box.WorldTransform.SetScale3D(FVector::OneVector);
	}
}

void ULagCompensationComponent::DrawHistoricalHitboxes(const FHistoricalFrame& Frame, const FColor& Color, float Duration) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World) return;

	for (const FHistoricalHitbox& Box : Frame.Hitboxes)
	{
		DrawDebugBox(World, Box.WorldTransform.GetLocation(), Box.BoxExtent, Box.WorldTransform.GetRotation(), Color, false, Duration, 0, 1.f);
	}
#endif
}

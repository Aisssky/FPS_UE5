// Copyright Aisssky


#include "Networking/HitValidationComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "AbilitySystem/Effects/GE_Damage.h"
#include "Combat/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "FPS/FPS.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/PlayerInterface.h"
#include "Networking/LagCompensationComponent.h"
#include "Networking/NetworkTimeSubsystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Tags/ShooterGameplayTags.h"
#include "Weapon/Weapon.h"

//控制台变量：联调时在 DS/服务器控制台切换，方便做 SSR 与当前世界 trace 的对比（测试矩阵 T2）
static TAutoConsoleVariable<int32> CVarSSREnable(
	TEXT("fps.SSR.Enable"), 1,
	TEXT("1=服务器用历史 hitbox 回溯判定命中；0=只用当前世界 trace"));

static TAutoConsoleVariable<int32> CVarSSRCompare(
	TEXT("fps.SSR.Compare"), 0,
	TEXT("1=每发同时跑当前世界 trace 并打印两者命中对比"));

static TAutoConsoleVariable<int32> CVarSSRDrawDebug(
	TEXT("fps.SSR.DrawDebug"), 0,
	TEXT("1=画出射线、历史 hitbox（黄）与当前 hitbox（绿），仅监听服务器/PIE 可见"));

static TAutoConsoleVariable<int32> CVarShotLog(
	TEXT("fps.Shot.Log"), 1,
	TEXT("1=每发打印 §17.1 诊断字段；0=只打印拒绝"));


UHitValidationComponent::UHitValidationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

	DamageEffectClass = UGE_Damage::StaticClass();
	OriginTolerance = 200.f;
	FireRateTolerance = 0.75f;
	HeadShotMultiplier = 1.f;
	RewindBroadPhaseRadius = 250.f;

	LastProcessedShotSequence = 0;
	bHasProcessedShot = false;
	LastAcceptedShotTime = 0.0;
	bHasAcceptedShot = false;
}

UCombatComponent* UHitValidationComponent::GetCombat() const
{
	return UCombatComponent::FindCombatComponent(GetOwner());
}

FShotResult& UHitValidationComponent::RejectShot(FShotResult& Result, EShotRejectReason Reason) const
{
	Result.bAccepted = false;
	Result.RejectReason = Reason;
	Result.HitActor = nullptr;
	return Result;
}

FShotResult UHitValidationComponent::ValidateAndProcessShot(const FShotRequest& Request, AController* RequestingController)
{
	FShotResult Result;
	Result.ShotSequence = Request.ShotSequence;

	const double ServerReceiveTime = GetWorld()->GetRealTimeSeconds();

	//只允许服务器执行
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		RejectShot(Result, EShotRejectReason::InvalidShooter);
		return Result;
	}

	//1~5：射手/武器/弹药/状态/序列号/方向/起点
	EShotRejectReason Reason = EShotRejectReason::None;
	if (!ValidateShotRequest(Request, RequestingController, Reason))
	{
		RejectShot(Result, Reason);
		LogShot(Request, Result, ServerReceiveTime);
		return Result;
	}

	//序列号通过即记为已处理：之后无论接受与否，同一序列号都不会再被处理第二次
	LastProcessedShotSequence = Request.ShotSequence;
	bHasProcessedShot = true;

	//6：时间戳换算
	//ClientShotTime==0 表示客户端还没完成时钟同步：降级为按收包时刻判定（不回溯，只会对客户端自己不利，不构成作弊点）
	UNetworkTimeSubsystem* NetTime = UNetworkTimeSubsystem::Get(this);
	double ServerShotTime = ServerReceiveTime;
	float EstimatedErrorMs = 0.f;
	const bool bHasClientTime = Request.ClientShotTime != 0.0;
	if (bHasClientTime && NetTime && !NetTime->ConvertClientShotTimeToServerTime(Request.ClientShotTime, ServerShotTime, EstimatedErrorMs))
	{
		RejectShot(Result, EShotRejectReason::TimestampTooOld);
		LogShot(Request, Result, ServerReceiveTime);
		return Result;
	}

	//7：未来/回溯窗口
	if (NetTime && !NetTime->IsShotTimeWithinAllowedWindow(ServerShotTime, ServerReceiveTime, Reason))
	{
		RejectShot(Result, Reason);
		LogShot(Request, Result, ServerReceiveTime);
		return Result;
	}
	Result.ValidatedServerShotTime = ServerShotTime;

	/*
	 * 射速 + 时间倒流校验（用校验过的射击时刻，而不是服务器收包时刻）
	 * 原因：Reliable RPC 在抖动下会成批到达，按收包时刻算会误杀正常连发
	 * 防作弊上界：时间戳必须单调、间隔 >= FireTime，且落在 [Now-MaxRewind, Now+Future] 窗口内，
	 * 所以作弊者最多一次性多打出「窗口长度 / FireTime」发，之后仍被射速限死
	 */
	AWeapon* Weapon = GetCombat()->CurrentWeapon;
	if (bHasAcceptedShot)
	{
		if (ServerShotTime + KINDA_SMALL_NUMBER < LastAcceptedShotTime)
		{
			RejectShot(Result, EShotRejectReason::TimestampTooOld);
			LogShot(Request, Result, ServerReceiveTime);
			return Result;
		}
		if (ServerShotTime - LastAcceptedShotTime < Weapon->FireTime * FireRateTolerance)
		{
			RejectShot(Result, EShotRejectReason::FireRateExceeded);
			LogShot(Request, Result, ServerReceiveTime);
			return Result;
		}
	}

	//8~9：回溯 trace（或降级为当前世界 trace）
	const bool bUseRewind = CVarSSREnable.GetValueOnGameThread() != 0;
	FHitResult CurrentHit;
	if (bUseRewind)
	{
		Result.bUsedRewind = true;
		PerformRewindTrace(Request, ServerShotTime, Result);
	}
	else
	{
		PerformCurrentWorldTrace(Request, CurrentHit);
		Result.HitActor = (IsValid(CurrentHit.GetActor()) && CurrentHit.GetActor()->Implements<UPlayerInterface>()) ? CurrentHit.GetActor() : nullptr;
		Result.HitboxName = CurrentHit.BoneName;
		Result.ImpactPoint = CurrentHit.ImpactPoint;
		Result.ImpactNormal = CurrentHit.ImpactNormal;
		Result.SurfaceType = CurrentHit.PhysMaterial.IsValid() ? CurrentHit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
	}

	//对比诊断：同一发子弹，回溯判定 vs 当前世界判定
	if (bUseRewind && CVarSSRCompare.GetValueOnGameThread() != 0)
	{
		PerformCurrentWorldTrace(Request, CurrentHit);
		AActor* CurrentActor = (IsValid(CurrentHit.GetActor()) && CurrentHit.GetActor()->Implements<UPlayerInterface>()) ? CurrentHit.GetActor() : nullptr;
		UE_LOG(LogTemp, Warning, TEXT("[HitValidation] Compare Seq=%u Rewind=%s(%s) Current=%s RewindAge=%.1fms"),
			Request.ShotSequence, *GetNameSafe(Result.HitActor), *Result.HitboxName.ToString(), *GetNameSafe(CurrentActor),
			(ServerReceiveTime - ServerShotTime) * 1000.0);
	}

	//10：接受（扣弹由 CombatComponent 在拿到 bAccepted=true 后执行，保证「接受」与「扣弹」同一时机）
	Result.bAccepted = true;
	LastAcceptedShotTime = ServerShotTime;
	bHasAcceptedShot = true;

	//11：命中则服务器应用伤害 GE
	if (Result.HasHit())
	{
		Result.bHeadShot = Result.HitboxName == "head";
		Result.Damage = Weapon->Damage * (Result.bHeadShot ? HeadShotMultiplier : 1.f);

		FHitResult DamageHit;
		DamageHit.ImpactPoint = Result.ImpactPoint;
		DamageHit.ImpactNormal = Result.ImpactNormal;
		DamageHit.BoneName = Result.HitboxName;
		Result.bLethal = ApplyDamageGameplayEffect(Result.HitActor, Result.Damage, &DamageHit);
	}

	//12：返回权威结果，由 CombatComponent 广播
	LogShot(Request, Result, ServerReceiveTime);
	return Result;
}

bool UHitValidationComponent::ValidateShotRequest(const FShotRequest& Request, AController* RequestingController, EShotRejectReason& OutRejectReason) const
{
	//1：射手/拥有权。Server RPC 本身只接受拥有连接的调用，这里再确认 Controller 确实控制着这个 Pawn
	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || !IsValid(RequestingController) || OwningPawn->GetController() != RequestingController)
	{
		OutRejectReason = EShotRejectReason::InvalidShooter;
		return false;
	}

	//已死亡不能开枪
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (ASC->HasMatchingGameplayTag(ShooterTags::TAG_State_Dead))
		{
			OutRejectReason = EShotRejectReason::InvalidShooter;
			return false;
		}
	}

	//2：武器已装备
	const UCombatComponent* Combat = GetCombat();
	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon))
	{
		OutRejectReason = EShotRejectReason::InvalidWeapon;
		return false;
	}

	//3：弹药 / 状态
	//主机/单机的本地玩家在调用 Server_RequestFire 之前 Local_Fire 已预扣一发，所以最后一发时这里是 0
	const bool bAmmoAlreadyPredicted = OwningPawn->IsLocallyControlled();
	if (Combat->CurrentWeapon->Ammo < (bAmmoAlreadyPredicted ? 0 : 1))
	{
		OutRejectReason = EShotRejectReason::NoAmmo;
		return false;
	}
	if (Combat->IsServerReloading())
	{
		OutRejectReason = EShotRejectReason::AbilityBlocked;
		return false;
	}

	//4：序列号（Reliable RPC 保序，所以必须严格递增）
	if (bHasProcessedShot && Request.ShotSequence <= LastProcessedShotSequence)
	{
		OutRejectReason = EShotRejectReason::DuplicateSequence;
		return false;
	}

	//5：方向有效（非零、无 NaN、归一化）
	const FVector Dir = Request.AimDirection;
	if (Dir.ContainsNaN() || !FMath::IsNearlyEqual(Dir.Size(), 1.f, 0.05f))
	{
		OutRejectReason = EShotRejectReason::InvalidDirection;
		return false;
	}

	//起点在服务器认定的眼睛位置附近
	FVector EyeLocation;
	FRotator EyeRotation;
	OwningPawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector Origin = Request.TraceOrigin;
	if (Origin.ContainsNaN() || FVector::DistSquared(Origin, EyeLocation) > FMath::Square(OriginTolerance))
	{
		OutRejectReason = EShotRejectReason::OriginOutOfTolerance;
		return false;
	}

	OutRejectReason = EShotRejectReason::None;
	return true;
}

bool UHitValidationComponent::PerformCurrentWorldTrace(const FShotRequest& Request, FHitResult& OutHit) const
{
	const UCombatComponent* Combat = GetCombat();
	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon)) return false;

	//与 M1 的服务器 trace 完全一致（只是从 CombatComponent 挪到这里）
	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActor(GetOwner());

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetAllChannels(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);

	const FVector Start = Request.TraceOrigin;
	const FVector End = Start + FVector(Request.AimDirection) * Combat->GetTraceLength();

	const bool bHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, FPSTraceChannels::ECC_Weapon,
		FCollisionShape::MakeSphere(Combat->CurrentWeapon->TraceRadius), QueryParams, ResponseParams);
	if (!bHit)
	{
		OutHit.ImpactPoint = End;
		OutHit.ImpactNormal = -FVector(Request.AimDirection);
	}
	return bHit;
}

bool UHitValidationComponent::PerformRewindTrace(const FShotRequest& Request, double ValidatedServerShotTime, FShotResult& OutResult) const
{
	const UCombatComponent* Combat = GetCombat();
	if (!IsValid(Combat) || !IsValid(Combat->CurrentWeapon)) return false;

	const FVector Start = Request.TraceOrigin;
	const FVector Dir = Request.AimDirection;
	const float TraceLength = Combat->GetTraceLength();
	const float TraceRadius = Combat->CurrentWeapon->TraceRadius;
	const bool bDrawDebug = CVarSSRDrawDebug.GetValueOnGameThread() != 0;

	/*
	 * 第一步：只对「世界」做当前时刻的 trace，得到遮挡距离
	 * 局限（§15.6）：动态门/移动平台/可移动掩体仍按当前状态判断，没有回溯
	 */
	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActor(GetOwner());

	FCollisionResponseParams WorldResponse;
	WorldResponse.CollisionResponse.SetAllChannels(ECR_Ignore);
	WorldResponse.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
	WorldResponse.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
	WorldResponse.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);

	FHitResult WorldHit;
	const FVector End = Start + Dir * TraceLength;
	const bool bWorldBlocked = GetWorld()->LineTraceSingleByChannel(WorldHit, Start, End, FPSTraceChannels::ECC_Weapon, QueryParams, WorldResponse);
	const float BlockDistance = bWorldBlocked ? WorldHit.Distance : TraceLength;

	//默认结果：打在墙上或打空
	OutResult.HitActor = nullptr;
	OutResult.ImpactPoint = bWorldBlocked ? WorldHit.ImpactPoint : End;
	OutResult.ImpactNormal = bWorldBlocked ? WorldHit.ImpactNormal : -Dir;
	OutResult.SurfaceType = (bWorldBlocked && WorldHit.PhysMaterial.IsValid()) ? WorldHit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;

	/*
	 * 第二步：对每个有历史记录的目标，取射击时刻的 hitbox，做射线 vs 有向盒的数学求交
	 * 只读历史数据，不移动真实角色（ADR-3），所以不会影响实时权威位置
	 */
	float BestDistance = BlockDistance;
	AActor* BestActor = nullptr;
	FName BestHitbox = NAME_None;

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Target = *It;
		if (!IsValid(Target) || Target == GetOwner()) continue;

		ULagCompensationComponent* LagComp = ULagCompensationComponent::FindLagCompensationComponent(Target);
		if (!LagComp) continue;

		//已死亡的目标不参与判定
		if (const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
		{
			if (TargetASC->HasMatchingGameplayTag(ShooterTags::TAG_State_Dead)) continue;
		}

		FHistoricalFrame Frame;
		if (!LagComp->GetHistoricalFrameAtTime(ValidatedServerShotTime, Frame))
		{
			//射击时刻目标还没有历史（刚出生），视为当时不可命中
			UE_LOG(LogTemp, Verbose, TEXT("[HitValidation] %s: %s"), *GetNameSafe(Target), *FPSNetValidation::RejectReasonToString(EShotRejectReason::TargetNotValidAtShotTime));
			continue;
		}

		//粗筛：射线离目标太远直接跳过
		const FVector TargetLocation = Frame.ActorTransform.GetLocation();
		if (FMath::PointDistToLine(TargetLocation, Dir, Start) > RewindBroadPhaseRadius) continue;

		for (const FHistoricalHitbox& Box : Frame.Hitboxes)
		{
			float Distance = 0.f;
			if (RayIntersectsHitbox(Start, Dir, BestDistance, Box, TraceRadius, Distance) && Distance < BestDistance)
			{
				BestDistance = Distance;
				BestActor = Target;
				BestHitbox = Box.HitboxName;
			}
		}

		if (bDrawDebug)
		{
			LagComp->DrawHistoricalHitboxes(Frame, FColor::Yellow, 3.f);
			FHistoricalFrame NowFrame;
			LagComp->BuildFrameNow(NowFrame);
			LagComp->DrawHistoricalHitboxes(NowFrame, FColor::Green, 3.f);
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, Start + Dir * BestDistance, BestActor ? FColor::Red : FColor::White, false, 3.f, 0, 0.5f);
	}
#endif

	if (!BestActor) return false;

	OutResult.HitActor = BestActor;
	OutResult.HitboxName = BestHitbox;
	OutResult.ImpactPoint = Start + Dir * BestDistance;
	OutResult.ImpactNormal = -Dir;
	OutResult.SurfaceType = SurfaceType1;

	//命中表面：取受害者该骨骼物理体的物理材质，让肉体特效和以前一样
	if (const ACharacter* VictimCharacter = Cast<ACharacter>(BestActor))
	{
		const ULagCompensationComponent* VictimLagComp = ULagCompensationComponent::FindLagCompensationComponent(BestActor);
		const FHitboxDefinition* Def = VictimLagComp ? VictimLagComp->HitboxDefinitions.FindByPredicate([&](const FHitboxDefinition& D) { return D.HitboxName == BestHitbox; }) : nullptr;
		const USkeletalMeshComponent* Mesh = VictimCharacter->GetMesh();
		const FBodyInstance* Body = (Def && Mesh) ? Mesh->GetBodyInstance(Def->BoneName) : nullptr;
		if (const UPhysicalMaterial* PhysMat = Body ? Body->GetSimplePhysicalMaterial() : nullptr)
		{
			OutResult.SurfaceType = PhysMat->SurfaceType;
		}
	}
	return true;
}

bool UHitValidationComponent::RayIntersectsHitbox(const FVector& Start, const FVector& Dir, float MaxDistance, const FHistoricalHitbox& Box, float Inflate, float& OutDistance)
{
	/*
	 * 射线 vs OBB：把射线变换到盒子局部空间，就变成射线 vs AABB 的 slab 测试
	 * 盒子各轴膨胀 TraceRadius，近似「半径为 TraceRadius 的球形 sweep」（与当前世界 trace 的手感对齐）
	 */
	const FVector LocalStart = Box.WorldTransform.InverseTransformPositionNoScale(Start);
	const FVector LocalDir = Box.WorldTransform.InverseTransformVectorNoScale(Dir);
	const FVector Extent = Box.BoxExtent + FVector(Inflate);

	double TMin = 0.0;
	double TMax = MaxDistance;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double O = LocalStart[Axis];
		const double D = LocalDir[Axis];
		const double E = Extent[Axis];

		if (FMath::Abs(D) < UE_SMALL_NUMBER)
		{
			//射线与该轴平行：起点不在 slab 内就不可能相交
			if (O < -E || O > E) return false;
			continue;
		}

		double T1 = (-E - O) / D;
		double T2 = (E - O) / D;
		if (T1 > T2) Swap(T1, T2);

		TMin = FMath::Max(TMin, T1);
		TMax = FMath::Min(TMax, T2);
		if (TMin > TMax) return false;
	}

	OutDistance = static_cast<float>(TMin);
	return true;
}

bool UHitValidationComponent::ApplyDamageGameplayEffect(AActor* Target, float Damage, const FHitResult* Hit) const
{
	//只走服务器权威路径
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	if (!IsValid(Target) || !DamageEffectClass || Damage <= 0.f) return false;

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC) return false;

	//已经死了：不再结算，也就不会重复触发死亡
	const bool bWasAlive = !TargetASC->HasMatchingGameplayTag(ShooterTags::TAG_State_Dead);
	if (!bWasAlive) return false;

	//来源 ASC = 射手的 ASC（PlayerState 上）；没有就退化为目标自己施加
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	UAbilitySystemComponent* SpecOwnerASC = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = SpecOwnerASC->MakeEffectContext();
	//Instigator=射手角色（HealthComponent/击杀计分用），EffectCauser=武器
	const UCombatComponent* Combat = GetCombat();
	Context.AddInstigator(GetOwner(), Combat ? static_cast<AActor*>(Combat->CurrentWeapon.Get()) : GetOwner());
	if (Hit)
	{
		Context.AddHitResult(*Hit);
	}

	FGameplayEffectSpecHandle SpecHandle = SpecOwnerASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid()) return false;

	//SetByCaller：负数 = 扣血
	SpecHandle.Data->SetSetByCallerMagnitude(ShooterTags::TAG_Data_Damage, -Damage);

	if (SourceASC)
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
	}
	else
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	}

	//致死判定来自 AttributeSet（它在 Health<=0 时打上 State.Dead）
	return TargetASC->HasMatchingGameplayTag(ShooterTags::TAG_State_Dead);
}

void UHitValidationComponent::LogShot(const FShotRequest& Request, const FShotResult& Result, double ServerReceiveTime) const
{
	if (Result.bAccepted && CVarShotLog.GetValueOnGameThread() == 0) return;

	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	const APlayerState* PS = OwningPawn ? OwningPawn->GetPlayerState() : nullptr;
	const UCombatComponent* Combat = GetCombat();
	const AWeapon* Weapon = Combat ? Combat->CurrentWeapon.Get() : nullptr;

	//§17.1 诊断字段
	UE_LOG(LogTemp, Warning,
		TEXT("[HitValidation] Player=%d Seq=%u Weapon=%s ClientShotTime=%.4f ServerReceiveTime=%.4f ValidatedShotTime=%.4f RewindAge=%.1fms Ping=%.0fms ")
		TEXT("Origin=%s Dir=%s Target=%s Hitbox=%s Impact=%s Damage=%.1f Lethal=%d Rewind=%d Accepted=%d Reason=%s"),
		PS ? PS->GetPlayerId() : -1,
		Request.ShotSequence,
		*GetNameSafe(Weapon),
		Request.ClientShotTime,
		ServerReceiveTime,
		Result.ValidatedServerShotTime,
		Result.bAccepted ? (ServerReceiveTime - Result.ValidatedServerShotTime) * 1000.0 : 0.0,
		PS ? PS->GetPingInMilliseconds() : 0.f,
		*FVector(Request.TraceOrigin).ToCompactString(),
		*FVector(Request.AimDirection).ToCompactString(),
		*GetNameSafe(Result.HitActor),
		*Result.HitboxName.ToString(),
		*FVector(Result.ImpactPoint).ToCompactString(),
		Result.Damage,
		Result.bLethal ? 1 : 0,
		Result.bUsedRewind ? 1 : 0,
		Result.bAccepted ? 1 : 0,
		*FPSNetValidation::RejectReasonToString(Result.RejectReason));
}

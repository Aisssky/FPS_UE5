// Copyright Druid Mechanics


#include "Combat/CombatComponent.h"

#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/WeaponData.h"
#include "Engine/Engine.h"
#include "FPS/FPS.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Weapon/Weapon.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Networking/HitValidationComponent.h"
#include "Networking/NetworkTimeSubsystem.h"
#include "Tags/ShooterGameplayTags.h"


UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	TraceLength = 20'000;
	bAiming = false;
	bTriggerPressed = false;
	Local_WeaponIndex = 0;
	bCycleEquipRequested = false;
	LocalShotSequence = 0;
	bServerReloading = false;
	ServerReloadTimeoutPadding = 0.25f;
}



void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled()) return;
	
	APlayerController* PC = Cast<APlayerController>(OwningPawn->GetController());
	if (!IsValid(PC)) return;
	
	FVector EyesWorldLocation;
	FRotator EyesWorldRotation;
	PC->GetActorEyesViewPoint(EyesWorldLocation, EyesWorldRotation);
	const FVector EyesWorldDirection = UKismetMathLibrary::GetForwardVector(EyesWorldRotation);
	
	const FVector Start = EyesWorldLocation;
	const FVector End = Start + EyesWorldDirection * TraceLength;
	
	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetAllChannels(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);
	
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, FPSTraceChannels::ECC_Weapon, QueryParams, ResponseParams);
	
	bHitPlayer = IsValid(Hit.GetActor()) && Hit.GetActor()->Implements<UPlayerInterface>();
	
	if (bHitPlayer != bHitPlayerLastFrame)
	{
		OnTargetingPlayerStatusChanged.Broadcast(bHitPlayer);
	}
	
	bHitPlayerLastFrame = bHitPlayer;
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UCombatComponent, Inventory);
	DOREPLIFETIME(UCombatComponent, CurrentWeapon);
	DOREPLIFETIME_CONDITION(UCombatComponent, bAiming, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UCombatComponent, CurrentReserveAmmo, COND_OwnerOnly);
}

void UCombatComponent::Initiate_CycleWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
	
	AdvanceWeaponIndex();
	Local_CycleWeapon(Local_WeaponIndex);
}

void UCombatComponent::Local_CycleWeapon(int32 WeaponIndex)
{
	AWeapon* NextWeapon = Inventory[WeaponIndex];
	if (!IsValid(NextWeapon) || !IsValid(WeaponData)) return;
	CurrentWeapon->WeaponStatus = EWeaponStatus::Cycling;
	NextWeapon->WeaponStatus = EWeaponStatus::Cycling;
	bCycleEquipRequested = false;
	
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bIsLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	
	const FMontageData& MontageData = bIsLocal ? WeaponData->FirstPersonMontages.FindChecked(NextWeapon->WeaponType) : WeaponData->ThirdPersonMontages.FindChecked(NextWeapon->WeaponType);
	USkeletalMeshComponent* Mesh = bIsLocal ? IPlayerInterface::Execute_GetMesh1P(GetOwner()) : IPlayerInterface::Execute_GetMesh3P(GetOwner());
	if (IsValid(Mesh) && IsValid(MontageData.EquipMontage))
	{
		Mesh->GetAnimInstance()->Montage_Play(MontageData.EquipMontage);
	}
	if (bIsLocal)
	{
		Server_CycleWeapon(WeaponIndex);
		Mesh->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this, &ThisClass::BlendOut_CycleWeapon);
	}
}

void UCombatComponent::Server_CycleWeapon_Implementation(int32 WeaponIndex)
{
	Local_WeaponIndex = WeaponIndex;
	Multicast_CycleWeapon(WeaponIndex);
}

void UCombatComponent::Multicast_CycleWeapon_Implementation(int32 WeaponIndex)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn)) return;
	
	if (!OwningPawn->IsLocallyControlled())
	{
		Local_WeaponIndex = WeaponIndex;
		Local_CycleWeapon(WeaponIndex);
	}
}

void UCombatComponent::Notify_CycleWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	
	AWeapon* NewWeapon = Inventory[Local_WeaponIndex];
	if (IsValid(NewWeapon))
	{
		bCycleEquipRequested = true;
		EquipWeapon(NewWeapon);
	}
}

void UCombatComponent::Notify_ReloadWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	//服务器：补弹统一走 Auth_CommitReload（与兜底计时器二选一，只生效一次）
	if (GetOwner()->HasAuthority())
	{
		Auth_CommitReload();
	}
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (IsValid(OwningPawn) && OwningPawn->IsLocallyControlled())
	{
		OnLocalReloadFinished.Broadcast();
	}

	if (bTriggerPressed && CurrentWeapon->Ammo > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::Auth_CommitReload()
{
	if (!bServerReloading) return;
	bServerReloading = false;
	GetWorld()->GetTimerManager().ClearTimer(ServerReloadTimer);

	//死亡/切枪导致武器没了：只解锁，不补弹
	if (!IsValid(CurrentWeapon) || !ReserveAmmo.Contains(CurrentWeapon->WeaponType)) return;

	//弹药唯一权威：服务器的 Weapon->Ammo + ReserveAmmo
	const int32 EmptySpace = CurrentWeapon->MagCapacity - CurrentWeapon->Ammo;
	const int32 AmountToRefill = FMath::Min(EmptySpace, ReserveAmmo[CurrentWeapon->WeaponType]);
	CurrentWeapon->Ammo += AmountToRefill;
	ReserveAmmo[CurrentWeapon->WeaponType] = ReserveAmmo[CurrentWeapon->WeaponType] - AmountToRefill;
	CurrentReserveAmmo = ReserveAmmo[CurrentWeapon->WeaponType];
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	Client_ReloadWeapon(CurrentWeapon->Ammo, CurrentReserveAmmo);
}

void UCombatComponent::AddAmmo(const FGameplayTag& WeaponType, int32 AmmoAmount)
{
	if (GetOwner()->HasAuthority() && !IsValid(CurrentWeapon)) return;
	
	if (!ReserveAmmo.Contains(WeaponType))
	{
		ReserveAmmo.Add(WeaponType, AmmoAmount);
	}
	
	const int32 NewAmmo = ReserveAmmo.FindChecked(WeaponType) + AmmoAmount;
	ReserveAmmo[WeaponType] = NewAmmo;
	
	if (CurrentWeapon->WeaponType.MatchesTagExact(WeaponType))
	{
		CurrentReserveAmmo = NewAmmo;
		if (CurrentWeapon->Ammo == 0 && NewAmmo > 0)
		{
			Server_ReloadWeapon();
		}
		
		OnAmmoCounterChanged.Broadcast(CurrentWeapon->GetAmmoCounterDynamicMaterialInstance(), CurrentWeapon->Ammo, CurrentWeapon->MagCapacity);
		OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);
	}
}

void UCombatComponent::Client_ReloadWeapon_Implementation(int32 NewWeaponAmmo, int32 NewCarriedAmmo)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(CurrentWeapon) || !IsValid(OwningPawn)) return;
	
	if (OwningPawn->IsLocallyControlled())
	{
		CurrentWeapon->Ammo = NewWeaponAmmo;
		CurrentReserveAmmo = NewCarriedAmmo;
		
		//服务器补弹到了就解除「换弹中」：不能只靠动画通知 Notify_ReloadWeapon 复位（ABP_FirstPerson 曾漏掉这个通知）
		if (CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading)
		{
			CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
		}
		
		OnAmmoCounterChanged.Broadcast(CurrentWeapon->GetAmmoCounterDynamicMaterialInstance(), CurrentWeapon->Ammo, CurrentWeapon->MagCapacity);
		OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);

		//服务器补弹到了：即使本地动画通知丢了（被打断），GA_Reload 也能结束
		OnLocalReloadFinished.Broadcast();
	}
}

void UCombatComponent::BlendOut_CycleWeapon(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimInstance* AnimInstance = IPlayerInterface::Execute_GetMesh1P(GetOwner())->GetAnimInstance();
	if (IsValid(AnimInstance) && AnimInstance->OnMontageBlendingOut.IsAlreadyBound(this, &ThisClass::BlendOut_CycleWeapon))
	{
		AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &ThisClass::BlendOut_CycleWeapon);
	}
	
	AWeapon* TargetWeapon = Inventory.IsValidIndex(Local_WeaponIndex) ? Inventory[Local_WeaponIndex] : nullptr;
	if (!bCycleEquipRequested && IsValid(TargetWeapon) && TargetWeapon != CurrentWeapon)
	{
		bCycleEquipRequested = true;
		EquipWeapon(TargetWeapon);
	}
	
	//新旧两把都复位：服务器 OnRep 回来之前 CurrentWeapon 还是旧枪
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	if (IsValid(TargetWeapon))
	{
		TargetWeapon->WeaponStatus = EWeaponStatus::Idle;
	}
	
	OnReticleChanged.Broadcast(CurrentWeapon->GetReticleDynamicMaterialInstance(), CurrentWeapon->ReticleParams, bHitPlayer);
	OnAmmoCounterChanged.Broadcast(CurrentWeapon->GetAmmoCounterDynamicMaterialInstance(), CurrentWeapon->Ammo, CurrentWeapon->MagCapacity);
	OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);
	
	if (bTriggerPressed && CurrentWeapon->FireType == EFireType::Auto && CurrentWeapon->Ammo > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::Initiate_FireWeapon_Pressed()
{
	if (!IsValid(CurrentWeapon)) return;
	
	bTriggerPressed = true;

	if (CanRequestFire())
	{
		Local_FireWeapon();
	}
}

bool UCombatComponent::CanRequestFire() const
{
	return IsValid(CurrentWeapon) && CurrentWeapon->WeaponStatus == EWeaponStatus::Idle && CurrentWeapon->Ammo > 0;
}

FShotRequest UCombatComponent::BuildShotRequest()
{
	FShotRequest Request;

	//服务器去重序号：只增不减，被拒的也照样消耗
	Request.ShotSequence = ++LocalShotSequence;

	//客户端「看到的画面」对应的服务器时间（协议见 NetworkTimeSubsystem.h 顶部）
	if (const UNetworkTimeSubsystem* NetTime = UNetworkTimeSubsystem::Get(this))
	{
		Request.ClientShotTime = NetTime->GetEstimatedViewedServerTime();
	}

	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (APlayerController* PC = IsValid(OwningPawn) ? Cast<APlayerController>(OwningPawn->GetController()) : nullptr)
	{
		FVector EyeLoc;
		FRotator EyeRot;
		PC->GetActorEyesViewPoint(EyeLoc, EyeRot);
		Request.TraceOrigin = EyeLoc;
		Request.AimDirection = EyeRot.Vector();
	}
	return Request;
}

void UCombatComponent::Local_FireWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	ensure(IsValid(WeaponData));
	
	CurrentWeapon->WeaponStatus = EWeaponStatus::Firing;
	
	UAnimMontage* Montage1P = WeaponData->FirstPersonMontages.FindChecked(CurrentWeapon->WeaponType).FireMontage;
	USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
	if (IsValid(Montage1P) && IsValid(Mesh1P))
	{
		Mesh1P->GetAnimInstance()->Montage_Play(Montage1P);
	}
	
	FHitResult Hit;
	CurrentWeapon->WeaponTrace(Hit, TraceLength);
	
	EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
	CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, true);
	
	OnRoundFired.Broadcast(CurrentWeapon->Ammo, CurrentWeapon->MagCapacity, CurrentReserveAmmo);
	
	GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireTimerFinished, CurrentWeapon->FireTime);
	
	//Server_FireWeapon(Hit);

	//只发意图，命中由服务器 HitValidation 裁决
	Server_RequestFire(BuildShotRequest());
}

int32 UCombatComponent::AdvanceWeaponIndex()
{
	if (Inventory.Num() >= 2)
	{
		Local_WeaponIndex = (Local_WeaponIndex + 1) % Inventory.Num();
	}
	return Local_WeaponIndex;
}

void UCombatComponent::FireTimerFinished()
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(CurrentWeapon) || !IsValid(OwningPawn)) return;
	
	if (CurrentWeapon->Ammo == 0 && CurrentReserveAmmo > 0 && OwningPawn->IsLocallyControlled())
	{
		//先把 Firing 状态放掉，否则 Initiate_ReloadWeapon 的状态检查会认为武器还在忙
		CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
		RequestReload();
		return;
	}

	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Firing)
	{
		CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	}
	
	if (bTriggerPressed && CurrentWeapon->FireType == EFireType::Auto && CurrentWeapon->Ammo > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::Server_RequestFire_Implementation(const FShotRequest& Request)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	UHitValidationComponent* HitValidation = UHitValidationComponent::FindHitValidationComponent(GetOwner());
	if (!IsValid(OwningPawn) || !ensure(HitValidation)) return;

	/*
	 * 校验/trace/命中/伤害全部交给 HitValidation（M6），这里只协调：
	 * 被拒 -> 通知客户端对账，不扣弹、不广播
	 * 接受 -> 扣弹 -> 计分 -> 广播特效
	 * Controller 取服务器已知的拥有关系，不信客户端传来的任何身份
	 */
	const FShotResult Result = HitValidation->ValidateAndProcessShot(Request, OwningPawn->GetController());
	if (!Result.bAccepted)
	{
		Client_ShotRejected(Request.ShotSequence, IsValid(CurrentWeapon) ? CurrentWeapon->Ammo : 0, Result.RejectReason);
		return;
	}

	CommitAuthoritativeAmmoCost();

	//击杀计分唯一入口（§4.1），bLethal 来自 GAS 死亡判定
	OnRoundReported.Broadcast(GetOwner(), Result.HitActor, Result.HasHit(), Result.bHeadShot, Result.bLethal);

	Multicast_FireWeapon(Result.ImpactPoint, Result.ImpactNormal, static_cast<uint8>(Result.SurfaceType.GetValue()), CurrentWeapon->Ammo);
}

void UCombatComponent::CommitAuthoritativeAmmoCost()
{
	if (!IsValid(CurrentWeapon)) return;

	/*
	 * 本地控制的 Pawn 在服务器上（ListenServer 主机 / 单机）时，Local_Fire 已扣过弹，这里不能再扣（见 done.md §9）
	 * 原守卫只判断了 ListenServer，单机下会双扣；DS 上 Pawn 永远不是本地控制，始终走权威扣弹
	 */
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled())
	{
		CurrentWeapon->Auth_Fire();
	}
}

void UCombatComponent::Client_ShotRejected_Implementation(uint32 ShotSequence, int32 AuthAmmo, EShotRejectReason Reason)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(CurrentWeapon) || !IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled()) return;

	UE_LOG(LogTemp, Warning, TEXT("[ShotTrace] Shot %u rejected: %s"), ShotSequence, *FPSNetValidation::RejectReasonToString(Reason));

	//这一发服务器没认：按「确认了一发、服务器弹药没变」对账，本地预测扣掉的那颗会被补回
	CurrentWeapon->Rep_Fire(AuthAmmo);
	OnAmmoCounterChanged.Broadcast(CurrentWeapon->GetAmmoCounterDynamicMaterialInstance(), CurrentWeapon->Ammo, CurrentWeapon->MagCapacity);
}

void UCombatComponent::Multicast_FireWeapon_Implementation(const FVector_NetQuantize& ImpactPoint, const FVector_NetQuantizeNormal& ImpactNormal, uint8 SurfaceType, int32 AuthAmmo)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || !IsValid(CurrentWeapon)) return;

	if (OwningPawn->IsLocallyControlled())
	{
		CurrentWeapon->Rep_Fire(AuthAmmo);
	}
	else
	{
		ensure(IsValid(WeaponData));

		CurrentWeapon->Local_Fire(ImpactPoint, ImpactNormal, static_cast<EPhysicalSurface>(SurfaceType), false);

		UAnimMontage* Montage3P = WeaponData->ThirdPersonMontages.FindChecked(CurrentWeapon->WeaponType).FireMontage;
		USkeletalMeshComponent* Mesh3P = IPlayerInterface::Execute_GetMesh3P(GetOwner());
		if (IsValid(Montage3P) && IsValid(Mesh3P))
		{
			Mesh3P->GetAnimInstance()->Montage_Play(Montage3P);
		}
	}
}

void UCombatComponent::Initiate_FireWeapon_Released()
{
	bTriggerPressed = false;
}

bool UCombatComponent::Initiate_ReloadWeapon()
{
	if (!IsValid(CurrentWeapon)) return false;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling || CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return false;
	if (CurrentWeapon->Ammo == CurrentWeapon->MagCapacity) return false;
	if (CurrentReserveAmmo == 0) return false;

	Local_ReloadWeapon();
	Server_ReloadWeapon();
	return true;
}

void UCombatComponent::RequestReload()
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		const FGameplayTagContainer ReloadTags(ShooterTags::TAG_Ability_Reload);
		if (ASC->TryActivateAbilitiesByTag(ReloadTags))
		{
			return;
		}
	}
	//没有 ASC / 没授予 GA_Reload 时的兜底
	Initiate_ReloadWeapon();
}

void UCombatComponent::Local_ReloadWeapon()
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(CurrentWeapon) || !IsValid(OwningPawn)) return;
	ensure(WeaponData);
	
	const bool bIsLocal = OwningPawn->IsLocallyControlled();
	UAnimMontage* ReloadMontage = bIsLocal ? WeaponData->FirstPersonMontages.FindChecked(CurrentWeapon->WeaponType).ReloadMontage : WeaponData->ThirdPersonMontages.FindChecked(CurrentWeapon->WeaponType).ReloadMontage;
	USkeletalMeshComponent* Mesh = bIsLocal ? IPlayerInterface::Execute_GetMesh1P(OwningPawn) : IPlayerInterface::Execute_GetMesh3P(OwningPawn);
	if (IsValid(ReloadMontage) && IsValid(Mesh))
	{
		Mesh->GetAnimInstance()->Montage_Play(ReloadMontage);
	}
	
	UAnimMontage* WeaponReloadMontage = WeaponData->WeaponMontages.FindChecked(CurrentWeapon->WeaponType).ReloadMontage;
	USkeletalMeshComponent* WeaponMesh = bIsLocal ? CurrentWeapon->GetMesh1P() : CurrentWeapon->GetMesh3P();
	if (IsValid(WeaponReloadMontage) && IsValid(WeaponMesh))
	{
		WeaponMesh->GetAnimInstance()->Montage_Play(WeaponReloadMontage);
	}
	CurrentWeapon->WeaponStatus = EWeaponStatus::Reloading;
}

void UCombatComponent::Server_ReloadWeapon_Implementation()
{
	//服务器校验：不信客户端的「我要换弹」，条件不满足就把权威弹药推回去让客户端对齐
	if (!IsValid(CurrentWeapon) || bServerReloading) return;
	const int32 ServerReserve = ReserveAmmo.Contains(CurrentWeapon->WeaponType) ? ReserveAmmo[CurrentWeapon->WeaponType] : 0;
	if (CurrentWeapon->Ammo >= CurrentWeapon->MagCapacity || ServerReserve <= 0)
	{
		Client_ReloadWeapon(CurrentWeapon->Ammo, ServerReserve);
		return;
	}

	bServerReloading = true;

	/*
	 * 不依赖客户端动画结束补弹（§12.5）：
	 * 正常情况由服务器自己的动画通知 Notify_ReloadWeapon 补弹；
	 * 动画被打断/DS 上动画不 Tick 时，兜底计时器到点也会补弹并解锁
	 */
	float Timeout = 2.f;
	if (IsValid(WeaponData))
	{
		if (const FMontageData* MontageData = WeaponData->ThirdPersonMontages.Find(CurrentWeapon->WeaponType); MontageData && IsValid(MontageData->ReloadMontage))
		{
			Timeout = MontageData->ReloadMontage->GetPlayLength();
		}
	}
	GetWorld()->GetTimerManager().SetTimer(ServerReloadTimer, this, &ThisClass::Auth_CommitReload, Timeout + ServerReloadTimeoutPadding, false);

	Multicast_ReloadWeapon();
}

void UCombatComponent::Multicast_ReloadWeapon_Implementation()
{
	//本地玩家已经在 Local_ReloadWeapon 里播过了，再播一次会把蒙太奇从头开始（通知被推迟一个 RTT）
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (IsValid(OwningPawn) && OwningPawn->IsLocallyControlled() && IsValid(CurrentWeapon) && CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading)
	{
		return;
	}
	Local_ReloadWeapon();
}

void UCombatComponent::Initiate_Aim_Pressed()
{
	Local_Aim(true);
	Server_Aim(true);
}

void UCombatComponent::Initiate_Aim_Released()
{
	Local_Aim(false);
	Server_Aim(false);
}

void UCombatComponent::Server_Aim_Implementation(bool bPressed)
{
	Local_Aim(bPressed);
}

void UCombatComponent::OnRep_CurrentReserveAmmo()
{
	if (IsValid(CurrentWeapon))
	{
		OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->Ammo, CurrentWeapon->WeaponIcon);
	}
}

void UCombatComponent::Local_Aim(bool bPressed)
{
	bAiming = bPressed;
	OnAimingStatusChanged.Broadcast(bAiming);
}


void UCombatComponent::Equip(AWeapon* Weapon)
{
	CurrentWeapon = Weapon;
	CurrentWeapon->AttachToOwningPawn(Cast<APawn>(GetOwner()));
	
	CurrentReserveAmmo = ReserveAmmo.FindChecked(CurrentWeapon->WeaponType);
	OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, Weapon->Ammo, CurrentWeapon->WeaponIcon);
}

void UCombatComponent::EquipWeapon(AWeapon* Weapon)
{
	if (!IsValid(Weapon) || !IsValid(GetOwner())) return;
	if (GetOwner()->GetLocalRole() == ROLE_Authority)
	{
		SetCurrentWeapon(Weapon, CurrentWeapon);
	}
	else
	{
		Server_EquipWeapon(Weapon);
	}
}

void UCombatComponent::Server_EquipWeapon_Implementation(AWeapon* Weapon)
{
	//已经是当前武器就忽略（重复请求会把当前武器先卸下再装上）
	if (Weapon == CurrentWeapon) return;
	EquipWeapon(Weapon);
}

void UCombatComponent::SetCurrentWeapon(AWeapon* NewWeapon, AWeapon* LastWeapon)
{
	AWeapon* LocalLastWeapon = nullptr;
	
	if (IsValid(LastWeapon))
	{
		LocalLastWeapon = LastWeapon;
	}
	else if (NewWeapon != CurrentWeapon)
	{
		LocalLastWeapon = CurrentWeapon;
	}
	
	if (IsValid(LocalLastWeapon))
	{
		LocalLastWeapon->DetachFromOwningPawn();
		LocalLastWeapon->WeaponStatus = EWeaponStatus::Unequipped;
	}
	
	CurrentWeapon = NewWeapon;
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn)) return;
	
	if (OwningPawn->HasAuthority() && IsValid(CurrentWeapon))
	{
		CurrentReserveAmmo = ReserveAmmo.FindChecked(CurrentWeapon->WeaponType);
	}
	if (!IsValid(CurrentWeapon)) return;
	CurrentWeapon->AttachToOwningPawn(OwningPawn);
	
	if (CurrentWeapon->Ammo == 0 && CurrentReserveAmmo > 0 && OwningPawn->IsLocallyControlled())
	{
		RequestReload();
	}
}

void UCombatComponent::SpawnInventory()
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority) return;
	
	for (TSubclassOf<AWeapon>& WeaponClass : DefaultWeaponClasses)
	{
		AWeapon* Weapon = SpawnWeapon(WeaponClass);
		Inventory.AddUnique(Weapon);
		ReserveAmmo.Add(Weapon->WeaponType, Weapon->StartingCarriedAmmo);
	}
	
	if (Inventory.Num() > 0)
	{
		Equip(Inventory[0]);
		InitializeWeaponWidgets();
	}
}

void UCombatComponent::DestroyInventory()
{
	for (AWeapon* Weapon : Inventory)
	{
		if (IsValid(Weapon))
		{
			Weapon->Destroy();
		}
	}
}

void UCombatComponent::InitializeWeaponWidgets() const
{
	if (IsValid(CurrentWeapon))
	{
		OnReticleChanged.Broadcast(CurrentWeapon->GetReticleDynamicMaterialInstance(), CurrentWeapon->ReticleParams, bHitPlayer);
		OnAmmoCounterChanged.Broadcast(CurrentWeapon->GetAmmoCounterDynamicMaterialInstance(), CurrentWeapon->Ammo, CurrentWeapon->MagCapacity);
	}
}

void UCombatComponent::OnRep_CurrentWeapon(AWeapon* LastWeapon)
{
	SetCurrentWeapon(CurrentWeapon, LastWeapon);
	
	IPlayerInterface::Execute_WeaponReplicated(GetOwner());
	InitializeWeaponWidgets();
}

AWeapon* UCombatComponent::SpawnWeapon(TSubclassOf<AWeapon> WeaponClass) const
{
	AActor* OwningActor = GetOwner();
	if (!IsValid(OwningActor)) return nullptr;
	if (OwningActor->GetLocalRole() < ROLE_Authority) return nullptr;
	
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Cast<APawn>(OwningActor);
	SpawnInfo.Owner = OwningActor;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	return GetWorld()->SpawnActor<AWeapon>(WeaponClass, SpawnInfo);
}



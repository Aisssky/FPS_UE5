// Copyright Druid Mechanics


#include "Character/ShooterCharacter.h"

#include "EnhancedInputComponent.h"
#include "TimerManager.h"
#include "AbilitySystem/FPSAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Combat/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/WeaponData.h"
#include "Elimination/EliminationComponent.h"
#include "FPS/FPS.h"
#include "Game/ShooterGameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Health/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Weapon/Weapon.h"
#include "AbilitySystem/FPSAbilitySystemComponent.h"
#include "AbilitySystem/FPSAttributeSet.h"
#include "AbilitySystem/Abilities/GA_FireWeapon.h"
#include "AbilitySystem/Abilities/GA_Reload.h"
#include "Networking/HitValidationComponent.h"
#include "Networking/LagCompensationComponent.h"
#include "Tags/ShooterGameplayTags.h"


AShooterCharacter::AShooterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	GetCharacterMovement()->MovementState.bCanCrouch = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 0.f;//弹簧臂长
	
	SpringArm->bEnableCameraLag = true;//启用延迟
	SpringArm->CameraLagSpeed = 15.f;//相机延迟速度
	
	SpringArm->bUsePawnControlRotation = true;
	
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>("FirstPersonCamera");
	FirstPersonCamera->SetupAttachment(SpringArm);
	FirstPersonCamera->bUsePawnControlRotation = false;
	
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>("Mesh1P");
	Mesh1P->SetupAttachment(FirstPersonCamera);
	
	//仅所有者可见
	Mesh1P->bOnlyOwnerSee = true;
	Mesh1P->bOwnerNoSee = false;
	
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->bReceivesDecals = false;
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;//渲染时才会更新Mesh,1P的更新只有所有者可见
	Mesh1P->PrimaryComponentTick.TickGroup = TG_PrePhysics;//确保Mesh更新在物理运算前进行
	
	GetMesh()->bOnlyOwnerSee = false;
	GetMesh()->bOwnerNoSee = true;
	GetMesh()->bReceivesDecals = false;
	
	Combat = CreateDefaultSubobject<UCombatComponent>("Combat");
	Combat->SetIsReplicated(true);
	
	Elimination = CreateDefaultSubobject<UEliminationComponent>("Elimination");
	Elimination->SetIsReplicated(false);
	
	Health = CreateDefaultSubobject<UHealthComponent>("Health");
	Health->SetIsReplicated(true);

	//只在服务器干活，不复制
	LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>("LagCompensation");
	HitValidation = CreateDefaultSubobject<UHitValidationComponent>("HitValidation");

	FireWeaponAbilityClass = UGA_FireWeapon::StaticClass();
	ReloadAbilityClass = UGA_Reload::StaticClass();

	DefaultFieldOfView = 90.0f;
	TurningStatus = ETurningInPlace::NotTurning;
	bWeaponFirstReplicated = false;
	bAbilitySystemInitialized = false;
	RespawnTime = 3.f;
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	Health->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
	FirstPersonCamera->SetFieldOfView(DefaultFieldOfView);
	StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
	
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()); IsValid(PC))
	{
		PC->bPawnAlive = true;
	}
	
	if (HasAuthority())
	{
		Combat->OnRoundReported.AddDynamic(Elimination, &UEliminationComponent::OnRoundReported);
	}
}

void AShooterCharacter::BeginDestroy()
{
	Super::BeginDestroy();
	
	if (IsValid(Combat))
	{
		Combat->DestroyInventory();
	}
}

FRotator AShooterCharacter::GetFixedAimRotation() const
{
	FRotator AimRotation = GetBaseAimRotation();
	if (AimRotation.Pitch > 90.f && !IsLocallyControlled())
	{
		const FVector2D InRange(270.f, 360.f);
		const FVector2D OutRange(-90.f, 0.f);
		AimRotation.Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AimRotation.Pitch);
	}
	
	return AimRotation;
}

bool AShooterCharacter::HasCurrentWeapon() const
{
	return IsValid(Combat) && Combat->CurrentWeapon != nullptr;
}

void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	CalculateTurnInPlaceParameters(DeltaTime);
	CalculateFABRIKSocketTransform();
}

void AShooterCharacter::CalculateTurnInPlaceParameters(float DeltaTime)
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	float Speed = Velocity.Size();
	bool bIsInAir = GetCharacterMovement()->IsFalling();
	
	//速度为0且不在空中的情况
	if (Speed == 0.f && !bIsInAir) 
	{
		//当前瞄准rotation
		FRotator CurrentAimRotation(0.f, GetBaseAimRotation().Yaw, 0.f);
		// StartingAimRotation initially set in BeginPlay
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		
		if (TurningStatus == ETurningInPlace::NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		
		TurnInPlace(DeltaTime); // interpolates the InterpAO_Yaw value to zero.
	}

	//奔跑 跳跃
	if (Speed > 0.f || bIsInAir)
	{
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		
		FRotator AimRotation = GetBaseAimRotation();
		FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(GetVelocity());
		MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw;
		TurningStatus = ETurningInPlace::NotTurning;
	}

	AO_Yaw *= -1.f;
}

void AShooterCharacter::TurnInPlace(float DeltaTime)
{
	
	/*偏航（Yaw）角色绕 Z 轴左右旋转的角度。正/负通常表示向右/向左偏。
偏移（Aim Offset / AO）角色上半身瞄准方向与下半身基础朝向之间的角度差。比如身体朝前，但瞄准方向向右偏了 80°，这个 80° 就是偏航偏移量，代码里对应 AO_Yaw。

平滑插值:不直接把数值瞬间改成目标值，而是每帧朝目标值靠近一点，让它逐渐过渡。

*/
	if (AO_Yaw > 90.f)
	{
		TurningStatus = ETurningInPlace::Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningStatus = ETurningInPlace::Left;
	}
	
	
	if (TurningStatus != ETurningInPlace::NotTurning) // we are turning
	{
		// 将插值用的偏航角 InterpAO_Yaw 平滑插值到 0，插值速度为 4.0
		// 这样可以让瞄准偏移逐渐归零，避免转身结束后偏移量仍然残留
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.0f);
		AO_Yaw = InterpAO_Yaw;
		// 当 AO_Yaw 的绝对值小于 5 度时，认为已经基本转正，转身完成
		if (FMath::Abs(AO_Yaw) < 5.f)
		{
			// 以当前基础瞄准旋转的 Yaw 作为新的起始瞄准旋转，Pitch 和 Roll 置零
			// 这样后续的瞄准偏移会基于新的朝向重新计算
			TurningStatus = ETurningInPlace::NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void AShooterCharacter::CalculateFABRIKSocketTransform()
{
	if (IsValid(Combat) && IsValid(Combat->CurrentWeapon) && IsValid(Combat->CurrentWeapon->GetMesh3P()))
	{
		FABRIK_SocketTransform = Combat->CurrentWeapon->GetMesh3P()->GetSocketTransform("FABRIK_Socket", RTS_World);
		
		FVector OutLocation;
		FRotator OutRotation;
		GetMesh()->TransformToBoneSpace(
			"hand_r", 
			FABRIK_SocketTransform.GetLocation(), 
			FABRIK_SocketTransform.GetRotation().Rotator(), 
			OutLocation, 
			OutRotation);
		FABRIK_SocketTransform.SetLocation(OutLocation);
		FABRIK_SocketTransform.SetRotation(OutRotation.Quaternion());
	}
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* ShooterInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	
	ShooterInputComponent->BindAction(CycleWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_CycleWeapon);
	ShooterInputComponent->BindAction(FireWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_FireWeapon_Pressed);
	ShooterInputComponent->BindAction(FireWeaponAction, ETriggerEvent::Completed, this, &ThisClass::Input_FireWeapon_Released);
	ShooterInputComponent->BindAction(AimWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_Aim_Pressed);
	ShooterInputComponent->BindAction(AimWeaponAction, ETriggerEvent::Completed, this, &ThisClass::Input_Aim_Released);
	ShooterInputComponent->BindAction(ReloadWeaponAction, ETriggerEvent::Started, this, &ThisClass::Input_ReloadWeapon);
}

void AShooterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (IsValid(Combat))
	{
		Combat->SpawnInventory();
	}
	
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()); IsValid(PC))
	{
		PC->bPawnAlive = true;
	}
	
	InitializeAbilitySystem();
}

void AShooterCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	if (IsValid(Combat))
	{
		Combat->InitializeWeaponWidgets();
	}
	
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()); IsValid(PC))
	{
		PC->bPawnAlive = true;
	}
	
	InitializeAbilitySystem(); 
}

FName AShooterCharacter::GetWeaponAttachPoint_Implementation(const FGameplayTag& WeaponType) const
{
	checkf(Combat->WeaponData, TEXT("No Weapon Data Asset - Please fill out BP_ShooterCharacter"));
	return Combat->WeaponData->GripPoints.FindChecked(WeaponType);//使用接口获取武器握点
}

USkeletalMeshComponent* AShooterCharacter::GetMesh1P_Implementation() const
{
	return Mesh1P;
}

USkeletalMeshComponent* AShooterCharacter::GetMesh3P_Implementation() const
{
	return GetMesh();
}

void AShooterCharacter::WeaponReplicated_Implementation()
{
	if (!bWeaponFirstReplicated)
	{
		bWeaponFirstReplicated = true;
		OnWeaponFirstReplicated.Broadcast(Combat->CurrentWeapon, Combat->bHitPlayer);
	}
}

AWeapon* AShooterCharacter::GetCurrentWeapon_Implementation()
{
	return Combat->CurrentWeapon;
}

int32 AShooterCharacter::GetReserveAmmo_Implementation() const
{
	return Combat->CurrentReserveAmmo;
}

void AShooterCharacter::Notify_CycleWeapon_Implementation()
{
	Combat->Notify_CycleWeapon();
}

void AShooterCharacter::Notify_ReloadWeapon_Implementation()
{
	Combat->Notify_ReloadWeapon();
}

void AShooterCharacter::AddAmmo_Implementation(const FGameplayTag& WeaponType, int32 AmmoAmount)
{
	if (HasAuthority() && IsValid(Combat))
	{
		Combat->AddAmmo(WeaponType, AmmoAmount);
	}
}

bool AShooterCharacter::DoDamage_Implementation(float DamageAmount, AActor* DamageInstigator)
{
	/*
	 * 过渡接口（M7）：射击伤害已改走 HitValidation -> GE_Damage，这里不再直接改 HealthComponent
	 * 保留给非射击来源（陷阱/掉落等）用，同样转发到 GAS，保证 Health 只有一个权威来源
	 */
	if (!HasAuthority()) return false;

	const UHitValidationComponent* Validator = UHitValidationComponent::FindHitValidationComponent(DamageInstigator);
	if (!Validator)
	{
		Validator = HitValidation;
	}
	return Validator ? Validator->ApplyDamageGameplayEffect(this, DamageAmount, nullptr) : false;
}

UAbilitySystemComponent* AShooterCharacter::GetAbilitySystemComponent() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent;
	}
	//还没初始化时直接问 PlayerState
	const AShooterPlayerState* PS = GetPlayerState<AShooterPlayerState>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

void AShooterCharacter::HandleHealthChanged(float NewHealth, float NewMaxHealth, float Delta, AActor* DamageInstigator)
{
	//HealthComponent 现在只是 GAS 的镜像：复制给 UI，不再自己算血
	if (IsValid(Health))
	{
		Health->SetHealthFromAttribute(NewHealth, NewMaxHealth, DamageInstigator);
	}

	//掉血但没死：播受击（死亡由 HandleOutOfHealth 处理）
	if (Delta < 0.f && NewHealth > 0.f && HitReacts.Num() > 0)
	{
		const int32 MontageSelection = FMath::RandRange(0, HitReacts.Num() - 1);
		Multicast_HitReact(MontageSelection);
	}
}

void AShooterCharacter::HandleOutOfHealth(AActor* DamageInstigator)
{
	//驱动原有死亡状态机：DeathState 复制 + OnDeathStarted（销毁武器、重生计时、关碰撞）
	if (IsValid(Health))
	{
		Health->StartDeath();
	}
}

void AShooterCharacter::Multicast_HitReact_Implementation(int32 MontageIndex)
{
	if (GetNetMode() != NM_DedicatedServer && !IsLocallyControlled())
	{
		if (HitReacts.IsValidIndex(MontageIndex))
		{
			GetMesh()->GetAnimInstance()->Montage_Play(HitReacts[MontageIndex]);
		}
	}
}

void AShooterCharacter::OnDeathStarted()
{
	//死亡打断所有 Ability，避免残留 State.Firing/State.Reloading
	if (AbilitySystemComponent && AbilitySystemComponent->GetAvatarActor() == this)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}
	//死后不再参与回溯判定，也不再记录
	if (IsValid(LagCompensation))
	{
		LagCompensation->ClearHistory();
		LagCompensation->SetComponentTickEnabled(false);
	}

	if (HasAuthority())
	{
		Combat->DestroyInventory();
		GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &ThisClass::DeathTimerFinished, RespawnTime);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		DeathEffects();
		if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetController()); IsValid(PC))
		{
			DisableInput(PC);
			if (PC->IsLocalController())
			{
				PC->bPawnAlive = false;
			}
		}
	}
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(FPSTraceChannels::ECC_Weapon, ECR_Ignore);
}

void AShooterCharacter::DeathTimerFinished()
{
	AShooterGameModeBase* GM = Cast<AShooterGameModeBase>(UGameplayStatics::GetGameMode(this));
	if (IsValid(GM))
	{
		GM->RequestRespawn(this, GetController());
	}
}

void AShooterCharacter::InitializeAbilitySystem()
{
	AShooterPlayerState* PS = GetPlayerState<AShooterPlayerState>();
	if (!PS)return;

	AbilitySystemComponent=Cast<UFPSAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	if (!AbilitySystemComponent)return;

	/*
	 * 幂等守卫：同一个 Character 被 PossessedBy/OnRep_PlayerState 多次调用时，只初始化一次
	 * 踩坑：最早用「ASC->GetAvatarActor()==this」当守卫，但 DS 实测客户端在 OnRep_PlayerState 之前
	 * 引擎就已经把 Avatar 绑成了这个角色，守卫直接 return，后面的初始化全被跳过。
	 * 所以改成角色自己的标记；重生是新的 Character，标记天然是 false
	 */
	if (bAbilitySystemInitialized)
	{
		return;
	}
	bAbilitySystemInitialized = true;

	//引擎可能已经绑过，没绑才绑
	if (AbilitySystemComponent->GetAvatarActor()!=this)
	{
		AbilitySystemComponent->InitAbilityActorInfo(PS,this);
	}

	AttributeSet=PS->GetAttributeSet();

	//授予/属性初始化只能在服务器做（客户端 GiveAbility 会被引擎拒绝）
	if (HasAuthority())
	{
		GrantDefaultAbilities();
		InitializeAttributesForSpawn();
	}

	UE_LOG(LogTemp, Warning, TEXT("[GAS] %s Init OK Owner=%s Avatar=%s ASC=%s Abilities=%d"),
	*GetName(),
	*AbilitySystemComponent->GetOwnerActor()->GetName(),
	*AbilitySystemComponent->GetAvatarActor()->GetName(),
	*AbilitySystemComponent->GetName(),
	AbilitySystemComponent->GetActivatableAbilities().Num());
}

void AShooterCharacter::GrantDefaultAbilities()
{
	/*
	 * ASC 在 PlayerState 上，重生后已授予的 Ability 还在
	 * 所以按「这个类有没有授予过」判断，而不是按 Avatar：否则每次重生都会多授予一份
	 */
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : { FireWeaponAbilityClass, ReloadAbilityClass })
	{
		if (AbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
		{
			FGameplayAbilitySpec Spec(AbilityClass,1,INDEX_NONE);
			AbilitySystemComponent->GiveAbility(Spec);
		}
	}
}

void AShooterCharacter::InitializeAttributesForSpawn()
{
	if (!AttributeSet) return;

	//清掉上一条命的死亡标记（复制的 Loose Tag，客户端也会被清掉）
	AbilitySystemComponent->SetLooseGameplayTagCount(ShooterTags::TAG_State_Dead, 0, EGameplayTagReplicationState::TagOnly);

	//初始血量沿用 HealthComponent 在蓝图里配置的 MaxHealth（只配一处）
	const float SpawnMaxHealth = IsValid(Health) ? Health->MaxHealth : 100.f;
	AbilitySystemComponent->SetNumericAttributeBase(UFPSAttributeSet::GetMaxHealthAttribute(), SpawnMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UFPSAttributeSet::GetHealthAttribute(), SpawnMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UFPSAttributeSet::GetStaminaAttribute(), AttributeSet->GetMaxStamina());

	if (IsValid(Health))
	{
		Health->SetHealthFromAttribute(SpawnMaxHealth, SpawnMaxHealth, nullptr);
	}
}

void AShooterCharacter::Input_CycleWeapon()
{
	Combat->Initiate_CycleWeapon();
}

void AShooterCharacter::Input_ReloadWeapon()
{
	if (AbilitySystemComponent && ReloadAbilityClass && AbilitySystemComponent->TryActivateAbilityByClass(ReloadAbilityClass))
	{
		return;
	}
	if (!AbilitySystemComponent)
	{
		Combat->Initiate_ReloadWeapon();
	}
}

void AShooterCharacter::Input_FireWeapon_Pressed()
{
	//按下 = 激活 GA_FireWeapon（换弹中/死亡时会被 Tag 挡住）
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbilityByClass(FireWeaponAbilityClass);
		return;
	}
	//ASC 还没初始化（PlayerState 尚未复制到）时的兜底
	Combat->Initiate_FireWeapon_Pressed();
}

void AShooterCharacter::Input_FireWeapon_Released()
{
	//松开 = 结束 GA_FireWeapon（EndAbility 里会放开扳机）
	if (AbilitySystemComponent)
	{
		const FGameplayTagContainer FireTags(ShooterTags::TAG_Ability_Fire);
		AbilitySystemComponent->CancelAbilities(&FireTags);
	}
	//兜底：Ability 没激活成功时扳机状态也要复位
	Combat->Initiate_FireWeapon_Released();
}

void AShooterCharacter::Input_Aim_Pressed()
{
	Combat->Initiate_Aim_Pressed();
	OnAim(true);
}

void AShooterCharacter::Input_Aim_Released()
{
	Combat->Initiate_Aim_Released();
	OnAim(false);
}

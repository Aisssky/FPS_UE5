// Copyright Druid Mechanics


#include "Player/ShooterPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Networking/NetworkTimeSubsystem.h"
#include "TimerManager.h"

AShooterPlayerController::AShooterPlayerController()
{
	bReplicates = true;
	bPawnAlive = true;
	TimeSyncInterval = 0.5f;
}

void AShooterPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	OnPlayerStateReplicated.Broadcast();
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem))
	{
		Subsystem->AddMappingContext(ShooterIMC, 0);
	}
	
	//只有远端客户端需要同步时钟（服务器/单机自己就是时间基准）；客户端上只存在自己的 PC
	if (GetNetMode() == NM_Client)
	{
		SendTimeSyncRequest();
		GetWorldTimerManager().SetTimer(TimeSyncTimer, this, &ThisClass::SendTimeSyncRequest, TimeSyncInterval, true);
	}
}

void AShooterPlayerController::SendTimeSyncRequest()
{
	if (UNetworkTimeSubsystem* NetTime = UNetworkTimeSubsystem::Get(this))
	{
		Server_RequestServerTime(NetTime->GetLocalTime());
	}
}

void AShooterPlayerController::Server_RequestServerTime_Implementation(double ClientSendTime)
{
	//服务器只负责回填自己的时间，不做任何换算
	const UWorld* World = GetWorld();
	Client_ReportServerTime(ClientSendTime, World ? World->GetRealTimeSeconds() : 0.0);
}

void AShooterPlayerController::Client_ReportServerTime_Implementation(double ClientSendTime, double ServerTime)
{
	if (UNetworkTimeSubsystem* NetTime = UNetworkTimeSubsystem::Get(this))
	{
		NetTime->RecordRoundTripSample(ClientSendTime, ServerTime, NetTime->GetLocalTime());
	}
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	UEnhancedInputComponent* ShooterInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);
	ShooterInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	ShooterInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	ShooterInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::Input_Jump);
	ShooterInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ThisClass::Input_Crouch);
}

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	bPawnAlive = true;
}

void AShooterPlayerController::Input_Crouch()
{
	// 不能用 Character 作变量名：AController 已有同名成员，会触发 C4458（项目将警告视为错误）
	ACharacter* ControlledCharacter = GetCharacter();
	if (!IsValid(ControlledCharacter)) return;
	if (!bPawnAlive) return;

	UCharacterMovementComponent* CMC = ControlledCharacter->GetCharacterMovement();
	if (!IsValid(CMC)) return;

	// 临时诊断：确认本端蹲伏能力是否开启、当前处于什么状态（验证后可删）
	UE_LOG(LogTemp, Warning, TEXT("[Crouch] INPUT %s CanEverCrouch=%d CanCrouch=%d IsCrouched=%d Wants=%d Role=%d Auth=%d"),
		*GetName(),
		CMC->CanEverCrouch() ? 1 : 0,
		ControlledCharacter->CanCrouch() ? 1 : 0,
		ControlledCharacter->bIsCrouched ? 1 : 0,
		CMC->bWantsToCrouch ? 1 : 0,
		static_cast<int32>(ControlledCharacter->GetLocalRole()),
		ControlledCharacter->HasAuthority() ? 1 : 0);

	// 判断用 bWantsToCrouch（标准入口里立即写入），保证快速连按也能正确反向；
	// 若用 bIsCrouched 判断，CMC 要到下一帧才处理请求，连按会重复发出同一个方向。
	// 执行走标准入口：Crouch()/UnCrouch() 会做 CanCrouch 检查、维护状态并触发 OnStart/EndCrouch
	// （远端靠 bIsCrouched 复制触发）。裸写 bWantsToCrouch 会被服务器 CanEverCrouch() 一票否决。
	if (CMC->bWantsToCrouch)
	{
		ControlledCharacter->UnCrouch();
	}
	else
	{
		ControlledCharacter->Crouch();
	}
}

void AShooterPlayerController::Input_Jump()
{
	ACharacter* ControlledCharacter = GetCharacter();
	if (!IsValid(ControlledCharacter)) return;
	if (!bPawnAlive) return;

	UCharacterMovementComponent* CMC = ControlledCharacter->GetCharacterMovement();
	if (!IsValid(CMC)) return;

	// 蹲伏时按跳先起立：同样走标准入口，避免只把 bWantsToCrouch 置 false 而胶囊/状态不同步
	if (CMC->bWantsToCrouch)
	{
		ControlledCharacter->UnCrouch();
	}
	else
	{
		ControlledCharacter->Jump();
	}
}

void AShooterPlayerController::Input_Move(const FInputActionValue& InputActionValue)
{
	if (!bPawnAlive) return;
	
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
	
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void AShooterPlayerController::Input_Look(const FInputActionValue& InputActionValue)
{
	if (!bPawnAlive) return;
	
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	AddYawInput(InputAxisVector.X);
	AddPitchInput(InputAxisVector.Y);
}

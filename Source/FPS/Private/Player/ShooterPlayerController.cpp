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
	if (!IsValid(GetCharacter())) return;
	if (!bPawnAlive) return;
	
	if (UCharacterMovementComponent* CMC = GetCharacter()->GetCharacterMovement(); IsValid(CMC))
	{
		CMC->bWantsToCrouch = !CMC->bWantsToCrouch;
	}
}

void AShooterPlayerController::Input_Jump()
{
	if (!IsValid(GetCharacter())) return;
	if (!bPawnAlive) return;
	
	UCharacterMovementComponent* CMC = GetCharacter()->GetCharacterMovement();
	if (!IsValid(CMC)) return;
	
	if (CMC->bWantsToCrouch)
	{
		CMC->bWantsToCrouch = false;
	}
	else
	{
		GetCharacter()->Jump();
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

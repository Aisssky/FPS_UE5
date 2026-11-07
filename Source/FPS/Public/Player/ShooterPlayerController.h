// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerStateReplicated);

UCLASS()
class FPS_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AShooterPlayerController();
	
	UPROPERTY(BlueprintAssignable)
	FPlayerStateReplicated OnPlayerStateReplicated;
	
	virtual void OnRep_PlayerState() override;
	
	bool bPawnAlive;
	
	//时间同步采样间隔（秒），M4 NetworkTime 用
	UPROPERTY(EditDefaultsOnly, Category = "FPS|NetworkTime")
	float TimeSyncInterval;
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	
	/*
	 * 时间同步往返：客户端带上本地发送时间 -> 服务器回填服务器时间 -> 客户端算 RTT/offset
	 * 用 Unreliable：丢一个样本无所谓，Reliable 重传反而会把 RTT 测大
	 */
	UFUNCTION(Server, Unreliable)
	void Server_RequestServerTime(double ClientSendTime);
	
	UFUNCTION(Client, Unreliable)
	void Client_ReportServerTime(double ClientSendTime, double ServerTime);
	
private:
	
	FTimerHandle TimeSyncTimer;
	void SendTimeSyncRequest();

	
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputMappingContext> ShooterIMC;
	
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> MoveAction;
	
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> LookAction;
	
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CrouchAction;
	
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> JumpAction;
	
	void Input_Crouch();
	void Input_Jump();
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
};

// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/PlayerInterface.h"
#include "ShooterTypes/ShooterTypes.h"
#include "ShooterCharacter.generated.h"

class UFPSAttributeSet;
class UGameplayAbility;
class UFPSAbilitySystemComponent;
class UEliminationComponent;
class ULagCompensationComponent;
class UHitValidationComponent;
class UHealthComponent;
class UCombatComponent;
class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class AWeapon;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWeaponFirstReplicated, AWeapon*, Weapon, bool, bTargetingPlayer);

UCLASS()
class FPS_API AShooterCharacter : public ACharacter, public IPlayerInterface, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AShooterCharacter();
	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	
	/** PlayerInterface */
	virtual FName GetWeaponAttachPoint_Implementation(const FGameplayTag& WeaponType) const override;
	virtual USkeletalMeshComponent* GetMesh1P_Implementation() const override;
	virtual USkeletalMeshComponent* GetMesh3P_Implementation() const override;
	virtual void WeaponReplicated_Implementation() override;
	virtual AWeapon* GetCurrentWeapon_Implementation() override;
	virtual int32 GetReserveAmmo_Implementation() const override;
	virtual void Notify_CycleWeapon_Implementation() override;
	virtual void Notify_ReloadWeapon_Implementation() override;
	virtual void AddAmmo_Implementation(const FGameplayTag& WeaponType, int32 AmmoAmount) override;
	virtual bool DoDamage_Implementation(float DamageAmount, AActor* DamageInstigator) override;
	/** ~PlayerInterface */

	//ASC 挂在 PlayerState 上，角色只是转发；让 UAbilitySystemGlobals 能从角色找到 ASC（伤害 GE、按标签激活都靠它）
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//由 UFPSAttributeSet::PostGameplayEffectExecute 调用（仅服务器）：把 GAS 的 Health 同步给 HealthComponent（UI/复制用）
	void HandleHealthChanged(float NewHealth, float NewMaxHealth, float Delta, AActor* DamageInstigator);

	//由 UFPSAttributeSet 死亡判定调用（仅服务器，只会调用一次）
	void HandleOutOfHealth(AActor* DamageInstigator);

	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	
	//偏航角问题
	UFUNCTION(BlueprintCallable)
	FRotator GetFixedAimRotation() const;
	
	//IK
	UPROPERTY(BlueprintReadOnly, Category = "FPS|FABRIK")
	FTransform FABRIK_SocketTransform;
	
	
	//状态机bool
	UFUNCTION(BlueprintCallable)
	bool HasCurrentWeapon() const;
	
	UPROPERTY(BlueprintAssignable)
	FWeaponFirstReplicated OnWeaponFirstReplicated;
	
	bool HasWeaponFirstReplicated() const { return bWeaponFirstReplicated; }
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|HitReact")
	TArray<TObjectPtr<UAnimMontage>> HitReacts;
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Respawn")
	float RespawnTime;
	
	//默认就是 UGA_FireWeapon，蓝图可以换成子类
	UPROPERTY(EditDefaultsOnly, Category = "FPS|AbilitySystem")
	TSubclassOf<UGameplayAbility> FireWeaponAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|AbilitySystem")
	TSubclassOf<UGameplayAbility> ReloadAbilityClass;

protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|AbilitySystem")
	TObjectPtr<UFPSAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|AbilitySystem")
	TObjectPtr<UFPSAttributeSet> AttributeSet;
	
	// 1st person view (arms)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Mesh")
	TObjectPtr<USkeletalMeshComponent> Mesh1P;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Health")
	TObjectPtr<UHealthComponent> Health;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Combat")
	TObjectPtr<UCombatComponent> Combat;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Elimination")
	TObjectPtr<UEliminationComponent> Elimination;

	//M5：服务器记录本角色历史 hitbox
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Networking")
	TObjectPtr<ULagCompensationComponent> LagCompensation;

	//M6：服务器校验本角色发出的射击请求
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Networking")
	TObjectPtr<UHitValidationComponent> HitValidation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Aiming")
	float DefaultFieldOfView;
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnAim(bool bIsAiming);
	
	UPROPERTY(BlueprintReadOnly, Category = "FPS|TurnInPlace")
	float AO_Yaw;
	
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Strafing")
	float MovementOffsetYaw;
	
	UPROPERTY(BlueprintReadOnly, Category = "FPS|TurnInPlace")
	ETurningInPlace TurningStatus;
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HitReact(int32 MontageIndex);
	
	UFUNCTION()
	void OnDeathStarted();
	
	UFUNCTION(BlueprintImplementableEvent)
	void DeathEffects();
	
	UFUNCTION()
	void InitializeAbilitySystem();

	//仅服务器：授予默认 Ability（已授予过就跳过，ASC 在 PlayerState 上跨重生存活）
	void GrantDefaultAbilities();

	//仅服务器：每次出生重置属性与死亡标记
	void InitializeAttributesForSpawn();

	
private:
	
	void Input_CycleWeapon();
	void Input_ReloadWeapon();
	void Input_FireWeapon_Pressed();
	void Input_FireWeapon_Released();
	void Input_Aim_Pressed();
	void Input_Aim_Released();
	
	void CalculateFABRIKSocketTransform();
	void CalculateTurnInPlaceParameters(float DeltaTime);
	void TurnInPlace(float DeltaTime);
	
	bool bWeaponFirstReplicated;
	//本角色是否已完成 ASC 初始化（InitializeAbilitySystem 的幂等守卫）
	bool bAbilitySystemInitialized;
	FRotator StartingAimRotation;
	float InterpAO_Yaw;
	FTimerHandle DeathTimer;
	
	void DeathTimerFinished();
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> SpringArm;
	
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CycleWeaponAction;
	
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> FireWeaponAction;
	
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> ReloadWeaponAction;
	
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> AimWeaponAction;
};

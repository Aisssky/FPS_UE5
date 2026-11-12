// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Combat/ShotRequest.h"
#include "Combat/ShotResult.h"
#include "CombatComponent.generated.h"


class UMaterialInstanceDynamic;
class UWeaponData;
class AWeapon;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FReticleChanged, UMaterialInstanceDynamic*, ReticleDynMatInst, const FReticleParams&, ReticleParams, bool, bCurrentlyTargetingPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAmmoCounterChanged, UMaterialInstanceDynamic*, AmmoCounterDynMatInst, int32, RoundsCurrent, int32, RoundsMax);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRoundFired, int32, RoundsCurrent, int32, RoundsMax, int32, RoundsInReserve);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAimingStatusChanged, bool, bIsAiming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTargetingPlayerStatusChanged, bool, bIsAiming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCurrentReserveAmmoChanged, int32, RoundsInReserve, int32, RoundsInWeapon, UMaterialInterface*, WeaponIconMaterial);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FRoundReported, AActor*, Attacker, AActor*, Victim, bool, bHit, bool, bHeadShot, bool, bLethal);
//本地换弹结束（GA_Reload 等它来结束 Ability、移除 State.Reloading）
DECLARE_MULTICAST_DELEGATE(FLocalReloadFinished);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	static UCombatComponent* FindCombatComponent(const AActor* Actor) { return ( IsValid(Actor) ? Actor->FindComponentByClass<UCombatComponent>() : nullptr ); }
	
	// Cycle to the next weapon in the inventory
	void Initiate_CycleWeapon();
	void Initiate_FireWeapon_Pressed();
	void Initiate_FireWeapon_Released();
	//返回 true 表示真的开始换弹了（GA_Reload 据此决定是否继续等待）
	bool Initiate_ReloadWeapon();
	//自动换弹统一入口：优先激活 GA_Reload（让 State.Reloading 生效），没有 ASC 时退回旧路径
	void RequestReload();
	void Initiate_Aim_Pressed();
	void Initiate_Aim_Released();
	
	void Notify_CycleWeapon();
	void Notify_ReloadWeapon();
	
	void AddAmmo(const FGameplayTag& WeaponType, int32 AmmoAmount);

	//S4-4：服务器——是否已拥有某 WeaponType 的武器（购买重复校验）
	bool HasWeaponOfType(const FGameplayTag& WeaponType) const;

	//S4-4：服务器——购买武器：spawn + 加入 Inventory + 装备（钱/阶段校验由 Character 做）
	void ServerAddPurchasedWeapon(TSubclassOf<AWeapon> WeaponClass);

	//S4-4：服务器——回合重置装备：销毁所有武器，重新只发默认武器（下回合重买）
	void ResetForNewRound();

	//S2-5：客户端当前散布半角（度）。准星扩散表现每帧读它；与服务器 HitValidation 读同一份 WeaponAccuracy，保证表现与判定一致
	float GetCurrentSpreadDegrees() const;
	
	UPROPERTY(BlueprintAssignable)
	FReticleChanged OnReticleChanged;
	
	UPROPERTY(BlueprintAssignable)
	FAmmoCounterChanged OnAmmoCounterChanged;
	
	UPROPERTY(BlueprintAssignable)
	FRoundFired OnRoundFired;
	
	UPROPERTY(BlueprintAssignable)
	FAimingStatusChanged OnAimingStatusChanged;
	
	UPROPERTY(BlueprintAssignable)
	FTargetingPlayerStatusChanged OnTargetingPlayerStatusChanged;
	
	UPROPERTY(BlueprintAssignable)
	FCurrentReserveAmmoChanged OnCurrentReserveAmmoChanged;
	
	UPROPERTY(BlueprintAssignable)
	FRoundReported OnRoundReported;
	
	FLocalReloadFinished OnLocalReloadFinished;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponData> WeaponData;
	
	void Equip(AWeapon* Weapon);
	void EquipWeapon(AWeapon* Weapon);
	
	UFUNCTION(Server, Reliable)
	void Server_EquipWeapon(AWeapon* Weapon);
	
	void SpawnInventory();
	void DestroyInventory();
	
	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bAiming;
	
	UPROPERTY(Transient, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AWeapon> CurrentWeapon;
	
	void InitializeWeaponWidgets() const;
	
	UPROPERTY(ReplicatedUsing = OnRep_CurrentReserveAmmo)
	int32 CurrentReserveAmmo;

	bool bHitPlayer;

	UFUNCTION(Server, Reliable)
	void Server_RequestFire(const FShotRequest& Request);
	
	//构造射击请求：序列号 + 时间采样 + 视角起点/方向（只构造，不做裁决）
	FShotRequest BuildShotRequest();
	
	//本地快速过滤（不能替代服务器校验）
	bool CanRequestFire() const;
	
	float GetTraceLength() const { return TraceLength; }
	
	//服务器视角的换弹状态（不依赖 WeaponStatus，那个在 DS 上不可靠）
	bool IsServerReloading() const { return bServerReloading; }
	
	
protected:
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	float TraceLength;
	
	UFUNCTION()
	void BlendOut_CycleWeapon(UAnimMontage* Montage, bool bInterrupted);
	
	//服务器换弹兜底超时 = 换弹蒙太奇时长 + 这个余量（动画通知没来也能补弹/解锁）
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	float ServerReloadTimeoutPadding;
	
private:
	TMap<FGameplayTag, int32> ReserveAmmo;
	bool bHitPlayerLastFrame;
	bool bTriggerPressed;
	FTimerHandle FireTimer;
	void FireTimerFinished();
	
	UFUNCTION()
	void OnRep_CurrentWeapon(AWeapon* LastWeapon);
	
	void SetCurrentWeapon(AWeapon* NewWeapon, AWeapon* LastWeapon);
	
	UPROPERTY(Transient, Replicated)
	TArray<AWeapon*> Inventory;
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	TArray<TSubclassOf<AWeapon>> DefaultWeaponClasses;
	
	AWeapon* SpawnWeapon(TSubclassOf<AWeapon> WeaponClass) const;
	
	UFUNCTION(Server, Reliable)
	void Server_Aim(bool bPressed);
	
	/*
	UFUNCTION(Server, Reliable)
	void Server_FireWeapon(const FHitResult& Hit);
	*/
	

	
	//只发特效需要的数据（命中点/法线/表面），不再把整个 FHitResult 广播出去
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FireWeapon(const FVector_NetQuantize& ImpactPoint, const FVector_NetQuantizeNormal& ImpactNormal, uint8 SurfaceType, int32 AuthAmmo);

	//远端玩家触发权威表现 Cue（S1-7）：弹着 + 枪声，走 GameplayCue 通道，本地不双播
	void TriggerFireCues(AActor* TargetActor, const FVector& ImpactPoint, const FVector& ImpactNormal, EPhysicalSurface SurfaceType);
	
	//服务器拒绝了这一发：客户端用权威弹药对账（等价于收到一次 Rep_Fire）
	UFUNCTION(Client, Reliable)
	void Client_ShotRejected(uint32 ShotSequence, int32 AuthAmmo, EShotRejectReason Reason);
	
	//仅服务器：接受射击后扣权威弹药
	void CommitAuthoritativeAmmoCost();
	
	//客户端射击序列号（每次 BuildShotRequest 自增）
	uint32 LocalShotSequence;
	
	//服务器换弹状态 + 兜底计时器
	bool bServerReloading;
	FTimerHandle ServerReloadTimer;
	//仅服务器：真正补弹（动画通知或兜底计时器触发，二者只生效一次）
	void Auth_CommitReload();
	
	UFUNCTION()
	void OnRep_CurrentReserveAmmo();
	
	void Local_Aim(bool bPressed);
	void Local_FireWeapon();

	//S2-3：本地应用后坐力（读 WeaponRecoil 表，按连发序号取该发 pitch/yaw 偏移叠加到视角）
	void ApplyLocalRecoil();

	//S2-3：当前连发序号（松手/换枪/换弹归零），索引 WeaponRecoil.Shots
	int32 RecoilShotIndex;
	
	int32 AdvanceWeaponIndex();
	int32 Local_WeaponIndex;
	
	//本次切枪是否已经发出过 EquipWeapon（动画通知或 BlendOut 兜底，二者只生效一次）
	bool bCycleEquipRequested;
	void Local_CycleWeapon(int32 WeaponIndex);
	
	UFUNCTION(Server, Reliable)
	void Server_CycleWeapon(int32 WeaponIndex);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CycleWeapon(int32 WeaponIndex);
	
	void Local_ReloadWeapon();
	
	UFUNCTION(Server, Reliable)
	void Server_ReloadWeapon();
	
	UFUNCTION(Client, Reliable)
	void Client_ReloadWeapon(int32 NewWeaponAmmo, int32 NewCarriedAmmo);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReloadWeapon();
};

// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterTypes/ShooterTypes.h"
#include "ShooterReticle.generated.h"

class AWeapon;
class UImage;

UCLASS()
class FPS_API UShooterReticle : public UUserWidget
{
	GENERATED_BODY()
public:

	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_Reticle;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_AmmoCounter;

	//S2-6：命中标记（BindWidget，蓝图里放一个 Image 命名为 Image_HitMarker）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_HitMarker;

	//S3-4：闪光致盲全屏白 overlay（灰盒表现，NativeTick 里读 State.Blinded 标签驱动淡入淡出）
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_BlindOverlay;

	//UMG 里继承的 BindWidget 变量不一定能在图表里直接搜到，给蓝图一个稳定的引用入口
	UFUNCTION(BlueprintPure, Category = "FPS|Reticle")
	UImage* GetReticleImage() const { return Image_Reticle.Get(); }

	UFUNCTION(BlueprintPure, Category = "FPS|Reticle")
	UImage* GetAmmoCounterImage() const { return Image_AmmoCounter.Get(); }

	UFUNCTION(BlueprintPure, Category = "FPS|Reticle")
	UImage* GetHitMarkerImage() const { return Image_HitMarker.Get(); }

	UFUNCTION(BlueprintPure, Category = "FPS|Reticle")
	UImage* GetBlindOverlayImage() const { return Image_BlindOverlay.Get(); }
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	//S2-6：命中标记表现事件（蓝图实现）：bHit 显示白十字、bHeadShot 红色、bLethal 击杀图标，C++ 只做转发
	UFUNCTION(BlueprintImplementableEvent, Category = "FPS|Reticle")
	void OnHitMarkerReceived(bool bHit, bool bHeadShot, bool bLethal);
private:
	
//创建动态材质
	TWeakObjectPtr<UMaterialInstanceDynamic> CurrentReticle_DynMatInst;
	TWeakObjectPtr<UMaterialInstanceDynamic> CurrentAmmoCounter_DynMatInst;
	FReticleParams CurrentReticleParams;
	float BaseCornerScaleFactor;
	float BaseShapeCutFactor;
	float _BaseCornerScaleFactor_RoundFired;
	float _BaseShapeCutFactor_RoundFired;
	float _BaseCornerScaleFactor_Aiming;
	float _BaseShapeCutFactor_Aiming;
	float _BaseCornerScaleFactor_TargetingPlayer;
	float _BaseCornerScaleFactor_Spread;
	bool bAiming;
	bool bTargetingPlayer;

	//S3-4：当前致盲白屏透明度（NativeTick 里读 State.Blinded 标签驱动）
	float CurrentBlindOpacity = 0.f;
	
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	
	UFUNCTION()
	void OnWeaponFirstReplicated(AWeapon* Weapon, bool bIsTargetingPlayer);
	
	UFUNCTION()
	void OnReticleChanged(UMaterialInstanceDynamic* ReticleDynMatInst, const FReticleParams& ReticleParams, bool bCurrentlyTargetingPlayer);
	
	UFUNCTION()
	void OnAmmoCounterChanged(UMaterialInstanceDynamic* AmmoCounterDynMatInst, int32 RoundsCurrent, int32 RoundsMax);
	
	UFUNCTION()
	void OnRoundFired(int32 RoundsCurrent, int32 RoundsMax, int32 RoundsInReserve);
	
	UFUNCTION()
	void OnAimingStatusChanged(bool bIsAiming);
	
	UFUNCTION()
	void OnTargetingPlayerStatusChanged(bool bTargeting);

	//S2-6：命中回传回调（服务器权威 bHit/bHeadShot/bLethal），转发给蓝图事件 OnHitMarkerReceived
	UFUNCTION()
	void OnRoundReported(AActor* Attacker, AActor* Victim, bool bHit, bool bHeadShot, bool bLethal);

	//S2-5：读当前 Pawn 的 CombatComponent 散布半角（度），无 Pawn/武器时返回 0
	float GetCurrentSpreadDegrees() const;
};

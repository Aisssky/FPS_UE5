// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FHealthChanged, UHealthComponent*, HealthComponent, float, OldValue, float, NewValue, AActor*, Instigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDeathStarted);

UENUM(BlueprintType)
enum class EDeathState : uint8
{
	NotDead,
	DeathStarted,
	DeathFinished
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintPure, Category = "FPS|Health")
	static UHealthComponent* FindHealthComponent(const AActor* Actor) { return IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr; }
	
	UFUNCTION(BlueprintCallable)
	float GetHealthNormalized() const;
	
	/*
	 * M7 起 Health 唯一权威是 GAS（UFPSAttributeSet），本组件降级为「镜像 + 死亡状态机」：
	 * - 旧的 ChangeHealthByAmount/ChangeMaxHealthByAmount 已删除，任何代码都不能再直接改这里的血量
	 * - Health/MaxHealth 仍复制给拥有者，现有 UI 绑定 OnHealthChanged 不用改
	 */

	//仅服务器：由 AShooterCharacter::HandleHealthChanged 把 GAS 的值同步过来
	void SetHealthFromAttribute(float NewHealth, float NewMaxHealth, AActor* Instigator);

	//仅服务器：由 GAS 死亡判定触发（内部有 DeathState 守卫，重复调用无害）
	void StartDeath();

	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	EDeathState DeathState;
	
	UPROPERTY(ReplicatedUsing = OnRep_Health, EditDefaultsOnly, Category = "FPS|Health")
	float Health;
	
	UPROPERTY(ReplicatedUsing = OnRep_MaxHealth, EditDefaultsOnly, Category = "FPS|Health")
	float MaxHealth;
	
	UPROPERTY(BlueprintAssignable)
	FHealthChanged OnHealthChanged;
	
	UPROPERTY(BlueprintAssignable)
	FHealthChanged OnMaxHealthChanged;
	
	UPROPERTY(BlueprintAssignable)
	FDeathStarted OnDeathStarted;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_DeathState(EDeathState OldDeathState);
	
	UFUNCTION()
	void OnRep_Health(float OldValue);
	
	UFUNCTION()
	void OnRep_MaxHealth(float OldValue);
};

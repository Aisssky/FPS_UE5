// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"


#include "FPSAttributeSet.generated.h"

UCLASS()
class FPS_API UFPSAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:

	UFPSAttributeSet();

	UPROPERTY(BlueprintReadOnly,Category="FPS|ASC|State",ReplicatedUsing=OnRep_Health)
	FGameplayAttributeData Health;

	//原来误绑成 OnRep_Health，导致 MaxHealth 变化时写回的是 Health 的预测缓存
	UPROPERTY(BlueprintReadOnly,Category="FPS|ASC|State",ReplicatedUsing=OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	
	UPROPERTY(BlueprintReadOnly,Category="FPS|ASC|State",ReplicatedUsing=OnRep_Stamina)
	FGameplayAttributeData Stamina;
	
	UPROPERTY(BlueprintReadOnly,Category="FPS|ASC|State",ReplicatedUsing=OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	
		
	ATTRIBUTE_ACCESSORS_BASIC(UFPSAttributeSet,Health);
	ATTRIBUTE_ACCESSORS_BASIC(UFPSAttributeSet,MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(UFPSAttributeSet,Stamina);
	ATTRIBUTE_ACCESSORS_BASIC(UFPSAttributeSet,MaxStamina);
	
	
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth)const;
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)const;
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldStamina)const;
	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)const;
	
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)override;

	//Health<=0 时触发死亡（唯一死亡入口），用 State.Dead 标签保证只触发一次
	void HandleDeathIfNeeded(const FGameplayEffectModCallbackData& Data);

};

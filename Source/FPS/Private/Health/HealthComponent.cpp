// Copyright Druid Mechanics


#include "Health/HealthComponent.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
	DeathState = EDeathState::NotDead;
	SetIsReplicatedByDefault(true);
	Health = 100.f;
	MaxHealth = 100.f;
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UHealthComponent, DeathState);
	DOREPLIFETIME_CONDITION(UHealthComponent, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHealthComponent, MaxHealth, COND_OwnerOnly);
}

float UHealthComponent::GetHealthNormalized() const
{
	return (MaxHealth > 0.f) ? (Health / MaxHealth) : 0.f;
}

void UHealthComponent::SetHealthFromAttribute(float NewHealth, float NewMaxHealth, AActor* Instigator)
{
	//只做镜像：值已经由 GAS clamp 过，这里不再计算
	if (!FMath::IsNearlyEqual(MaxHealth, NewMaxHealth))
	{
		const float OldMax = MaxHealth;
		MaxHealth = NewMaxHealth;
		OnMaxHealthChanged.Broadcast(this, OldMax, MaxHealth, Instigator);
	}

	const float OldValue = Health;
	Health = NewHealth;
	OnHealthChanged.Broadcast(this, OldValue, Health, Instigator);
}

void UHealthComponent::StartDeath()
{
	if (DeathState != EDeathState::NotDead)
	{
		return;
	}

	DeathState = EDeathState::DeathStarted;
	OnDeathStarted.Broadcast();
	GetOwner()->ForceNetUpdate();
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	
}

void UHealthComponent::OnRep_DeathState(EDeathState OldDeathState)
{
	if (DeathState == EDeathState::DeathStarted)
	{
		OnDeathStarted.Broadcast();
	}
}

void UHealthComponent::OnRep_Health(float OldValue)
{
	OnHealthChanged.Broadcast(this, OldValue, Health, nullptr);
}

void UHealthComponent::OnRep_MaxHealth(float OldValue)
{
	OnMaxHealthChanged.Broadcast(this, OldValue, MaxHealth, nullptr);
}


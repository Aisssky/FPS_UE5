// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Data/SpecialElimData.h"
#include "GameFramework/PlayerState.h"
#include "ShooterPlayerState.generated.h"


class USpecialElim;
class USpecialElimData;
enum class ESpecialElimType : uint16;
class UFPSAbilitySystemComponent;
class UFPSAttributeSet;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FScoreChanged, int32, NewScore);
//S4-4：金钱变化广播（客户端 UI 订阅，服务器直接改 Money 触发复制 + OnRep）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoneyChanged, int32, NewMoney);

UCLASS()
class FPS_API AShooterPlayerState : public APlayerState,public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UPROPERTY()
	TObjectPtr<UFPSAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UFPSAttributeSet> AttributeSet;
	
	UFPSAttributeSet* GetAttributeSet() const { return AttributeSet; }
	
	AShooterPlayerState();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//队伍 ID（S4-3）：0=未分配，1=攻击方，2=防守方；服务器在比赛开始分配
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "FPS|Team")
	int32 TeamID;

	//经济（S4-4）：击杀/回合胜负/败方补偿结算，购买武器/技能消耗；服务器权威
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Money, Category = "FPS|Economy")
	int32 Money;

	//金钱变化（客户端 UI 订阅，不轮询）
	UPROPERTY(BlueprintAssignable, Category = "FPS|Economy")
	FMoneyChanged OnMoneyChanged;

	UFUNCTION()
	void OnRep_Money();
	
	UPROPERTY(BlueprintAssignable)
	FScoreChanged OnScoreChanged;
	
	void AddScoredElim();
	void AddDefeat();
	void AddHit();
	void AddMiss();
	void AddHeadShotElim();
	void AddSequentialElim(int32 SequenceCount);
	void UpdateHighestStreak(int32 StreakCount);
	void AddRevengeElim();
	void AddDethroneElim();
	void AddShowStopperElim();
	void GotFirstBlood();
	void IsNowWinner();
	void SetOnStreak(bool bIsOnStreak);
	void SetLastAttacker(APlayerState* Attacker);
	
	bool IsOnStreak() const;
	APlayerState* GetLastAttacker() const;
	int32 GetScoredElims() const;
	
	UFUNCTION(Client, Reliable)
	void Client_LostTheLead();
	
	UFUNCTION(Client, Reliable)
	void Client_ScoredElim(int32 ElimScore);
	
	UFUNCTION(Client, Reliable)
	void Client_SpecialElim(const ESpecialElimType& SpecialElim, int32 SequentialElimCount, int32 StreakCount, int32 ElimScore);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|SpecialElims")
	TObjectPtr<USpecialElimData> SpecialElimData;
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|SpecialElims")
	TSubclassOf<USpecialElim> SpecialElimWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|SpecialElims")
	float ElimDisplayTime;
	
private:
	int32 ScoredElims;
	int32 Defeats;
	int32 Hits;
	int32 Misses;
	bool bOnStreak; // How many elims do we have since we've spawned?
	int32 HeadShotElims;
	TMap<int32, int32> SequentialElims; // Sequential elims - multiple elims within a short period of time
	int32 HighestStreak;
	int32 RevengeElims;
	int32 DethroneElims;
	int32 ShowStopperElims;
	bool bFirstBlood;
	bool bWinner;
	
	TWeakObjectPtr<APlayerState> LastAttacker;
	
	TArray<ESpecialElimType> DecodeElimBitmask(ESpecialElimType ElimTypeBitmask);
	void ProcessNextSpecialElim();
	void ShowSpecialElim(const FSpecialElimInfo& ElimMessageInfo);
	TQueue<FSpecialElimInfo> SpecialElimQueue;
	bool bIsProcessingQueue;
};

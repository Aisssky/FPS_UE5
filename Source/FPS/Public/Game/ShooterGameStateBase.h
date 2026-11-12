// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ShooterGameStateBase.generated.h"


class AShooterPlayerState;

/*
 * 回合状态（S4-1）：服务器权威推进，复制给客户端 HUD
 * Warmup 出生等待 -> Buy 购买阶段 -> RoundActive 对战 -> RoundEnd 结算 -> MatchEnd 比赛结束
 */
UENUM(BlueprintType)
enum class ERoundState : uint8
{
	Warmup,
	Buy,
	RoundActive,
	RoundEnd,
	MatchEnd
};

//S4-4：回合阶段变化广播（蓝图 UI 监听，Buy 阶段开购买菜单、离开关闭；事件驱动不轮询）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRoundStateChanged, ERoundState, NewState);

UCLASS()
class FPS_API AShooterGameStateBase : public AGameStateBase
{
	GENERATED_BODY()
public:
	AShooterGameStateBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	bool HasFirstBloodBeenHad() const;
	void UpdateLeader();
	AShooterPlayerState* GetSoleLeader() const;
	bool IsTiedForTheLead(AShooterPlayerState* PlayerState);

	//—— S4-2 回合状态复制（服务器写，客户端读）——

	//当前回合阶段
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RoundState, Category = "FPS|Round")
	ERoundState RoundState;

	//当前阶段剩余时间（秒），由 GameMode 每帧递减复制
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "FPS|Round")
	float RoundTime;

	//回合编号（从 1 开始）
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "FPS|Round")
	int32 RoundNumber;

	//队伍比分：Index 0 = 攻击方(Team 1)，Index 1 = 防守方(Team 2)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "FPS|Round")
	TArray<int32> TeamScores;

	//服务器：进入下一阶段（由 GameMode 调用）
	void ServerSetRoundState(ERoundState NewState);

	//服务器：设置阶段剩余时间
	void ServerSetRoundTime(float NewTime);

	//回合阶段变化（客户端 OnRep / Listen Server 本地都广播）
	UPROPERTY(BlueprintAssignable, Category = "FPS|Round")
	FRoundStateChanged OnRoundStateChanged;

	UFUNCTION()
	void OnRep_RoundState();

private:
	
	bool bHasFirstBloodBeenHad;
	
	UPROPERTY()
	TArray<TObjectPtr<AShooterPlayerState>> Leaders;
};

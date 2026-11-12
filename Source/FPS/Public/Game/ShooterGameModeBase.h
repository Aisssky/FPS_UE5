// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/ShooterGameStateBase.h"
#include "ShooterGameModeBase.generated.h"


/*
 * 回合制 GameMode（S4-1）：服务器权威回合状态机
 * Warmup -> Buy -> RoundActive -> RoundEnd -> (下一回合 Buy) -> MatchEnd
 * 2v2：攻击方(Team 1) vs 防守方(Team 2)，先拿够 MatchWinScore 的队伍获胜
 * 回合内死亡不复活；回合结束统一复活（覆盖旧的即时 RequestRespawn 语义）
 */
UCLASS()
class FPS_API AShooterGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
public:

	AShooterGameModeBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void RequestRespawn(ACharacter* Character, AController* Controller);

	//服务器：进入指定回合阶段（切换阶段计时）
	void EnterRoundState(ERoundState NewState);

	//服务器：玩家死亡（由 AttributeSet 死亡流程转到这里，做回合内不复活 + 回合结束判定）
	void OnPlayerDeath(AController* Killed, AController* Killer);

	//服务器：判断当前队伍剩余存活数
	int32 CountAliveOnTeam(int32 Team) const;

	//服务器：分配玩家到队伍（2v2：按加入顺序，1、3 号进攻击方，2、4 号进防守方）
	void AssignTeam(APlayerController* Player);

protected:
	//阶段时长（秒），蓝图可改
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Round")
	float WarmupTime;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Round")
	float BuyTime;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Round")
	float RoundActiveTime;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Round")
	float RoundEndTime;

	//获胜所需回合数
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Round")
	int32 MatchWinScore;

private:
	//当前阶段已过时间（秒）
	float CurrentStateElapsed;

	//下一回合要分配的队伍（交替：攻击/防守/攻击/防守）
	int32 NextTeamAssignment;

	//服务器：结算当前回合，判定胜负/更新比分
	void ResolveRoundEnd();

	//服务器：开始新回合（重置队伍经济、统一复活）
	void StartNewRound();

	//服务器：复活所有玩家到各自队伍出生点
	void RespawnAllPlayers();

	//服务器：更新 GameState 的 TeamScores
	void UpdateTeamScores();

	//服务器：按队伍找出生点
	AActor* FindPlayerStartForTeam(AController* Player, int32 Team) const;

	//服务器：给所有存活玩家补经济（回合胜负/败方补偿，简化版）
	void AwardRoundEconomy(int32 WinningTeam);

	AShooterGameStateBase* GetShooterGameState() const;
};

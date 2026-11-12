// Copyright Druid Mechanics


#include "Game/ShooterGameModeBase.h"

#include "AbilitySystemComponent.h"
#include "Character/ShooterCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/ShooterGameplayTags.h"
#include "TimerManager.h"

AShooterGameModeBase::AShooterGameModeBase()
{
	PrimaryActorTick.bCanEverTick = true;

	WarmupTime = 5.f;
	BuyTime = 20.f;
	RoundActiveTime = 100.f;
	RoundEndTime = 5.f;
	MatchWinScore = 7;

	CurrentStateElapsed = 0.f;
	NextTeamAssignment = 1;
}

void AShooterGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	//进入 Warmup
	EnterRoundState(ERoundState::Warmup);
}

void AShooterGameModeBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority()) return;

	AShooterGameStateBase* GS = GetShooterGameState();
	if (!GS) return;

	//阶段计时推进
	CurrentStateElapsed += DeltaSeconds;

	switch (GS->RoundState)
	{
	case ERoundState::Warmup:
		GS->ServerSetRoundTime(FMath::Max(0.f, WarmupTime - CurrentStateElapsed));
		if (CurrentStateElapsed >= WarmupTime)
		{
			EnterRoundState(ERoundState::Buy);
		}
		break;

	case ERoundState::Buy:
		GS->ServerSetRoundTime(FMath::Max(0.f, BuyTime - CurrentStateElapsed));
		if (CurrentStateElapsed >= BuyTime)
		{
			EnterRoundState(ERoundState::RoundActive);
		}
		break;

	case ERoundState::RoundActive:
		GS->ServerSetRoundTime(FMath::Max(0.f, RoundActiveTime - CurrentStateElapsed));
		if (CurrentStateElapsed >= RoundActiveTime)
		{
			//时间耗尽：进攻方未安放/未团灭 → 防守方赢
			ResolveRoundEnd();
		}
		break;

	case ERoundState::RoundEnd:
		GS->ServerSetRoundTime(FMath::Max(0.f, RoundEndTime - CurrentStateElapsed));
		if (CurrentStateElapsed >= RoundEndTime)
		{
			//判断比赛是否结束
			const int32 AtkScore = GS->TeamScores.Num() > 0 ? GS->TeamScores[0] : 0;
			const int32 DefScore = GS->TeamScores.Num() > 1 ? GS->TeamScores[1] : 0;
			if (AtkScore >= MatchWinScore || DefScore >= MatchWinScore)
			{
				EnterRoundState(ERoundState::MatchEnd);
			}
			else
			{
				StartNewRound();
			}
		}
		break;

	case ERoundState::MatchEnd:
		GS->ServerSetRoundTime(0.f);
		break;
	}
}

void AShooterGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority()) return;

	//分配队伍（2v2：轮流进攻击/防守）
	AssignTeam(NewPlayer);
}

void AShooterGameModeBase::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
}

void AShooterGameModeBase::EnterRoundState(ERoundState NewState)
{
	if (!HasAuthority()) return;

	AShooterGameStateBase* GS = GetShooterGameState();
	if (!GS) return;

	CurrentStateElapsed = 0.f;
	GS->ServerSetRoundState(NewState);

	UE_LOG(LogTemp, Warning, TEXT("[Round] EnterState=%d RoundNumber=%d"), static_cast<int32>(NewState), GS->RoundNumber);
}

void AShooterGameModeBase::AssignTeam(APlayerController* Player)
{
	if (!IsValid(Player)) return;

	AShooterPlayerState* PS = Player->GetPlayerState<AShooterPlayerState>();
	if (!PS) return;

	//2v2：1、3 号进攻击方(1)，2、4 号进防守方(2)；超出 4 人则重复
	const int32 Team = (NextTeamAssignment % 2 == 1) ? 1 : 2;
	PS->TeamID = Team;
	++NextTeamAssignment;

	UE_LOG(LogTemp, Warning, TEXT("[Round] Player=%s assigned Team=%d"), *GetNameSafe(PS), Team);
}

void AShooterGameModeBase::OnPlayerDeath(AController* Killed, AController* Killer)
{
	if (!HasAuthority()) return;

	//回合内不复活（S4-6）：死亡后不立刻 RequestRespawn，等回合结束统一复活
	//检查当前回合是否还有队伍存活
	if (GetShooterGameState()->RoundState != ERoundState::RoundActive) return;

	//判断是否某队团灭
	const int32 AliveTeam1 = CountAliveOnTeam(1);
	const int32 AliveTeam2 = CountAliveOnTeam(2);

	if (AliveTeam1 == 0 || AliveTeam2 == 0)
	{
		ResolveRoundEnd();
	}
}

int32 AShooterGameModeBase::CountAliveOnTeam(int32 Team) const
{
	int32 Count = 0;
	for (TActorIterator<AShooterCharacter> It(GetWorld()); It; ++It)
	{
		const AShooterCharacter* Character = *It;
		if (!IsValid(Character)) continue;

		const AShooterPlayerState* PS = Character->GetPlayerState<AShooterPlayerState>();
		if (!PS || PS->TeamID != Team) continue;

		//存活判定：没有 State.Dead 标签（死亡由 GAS 死亡流程挂上）
		const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
		if (ASC && !ASC->HasMatchingGameplayTag(ShooterTags::TAG_State_Dead))
		{
			++Count;
		}
	}
	return Count;
}

void AShooterGameModeBase::ResolveRoundEnd()
{
	if (!HasAuthority()) return;

	AShooterGameStateBase* GS = GetShooterGameState();
	if (!GS || GS->RoundState != ERoundState::RoundActive) return;

	//判定胜方：团灭判定 + 时间耗尽判定
	int32 WinningTeam = 0;
	const int32 AliveTeam1 = CountAliveOnTeam(1);
	const int32 AliveTeam2 = CountAliveOnTeam(2);

	if (AliveTeam1 == 0 && AliveTeam2 == 0)
	{
		//双团灭：防守方赢（进攻方没完成目标）
		WinningTeam = 2;
	}
	else if (AliveTeam1 == 0)
	{
		WinningTeam = 2;
	}
	else if (AliveTeam2 == 0)
	{
		WinningTeam = 1;
	}
	else
	{
		//都没团灭（时间耗尽）：防守方赢
		WinningTeam = 2;
	}

	//更新比分
	if (WinningTeam == 1 && GS->TeamScores.Num() > 0) GS->TeamScores[0]++;
	if (WinningTeam == 2 && GS->TeamScores.Num() > 1) GS->TeamScores[1]++;

	UE_LOG(LogTemp, Warning, TEXT("[Round] RoundEnd WinningTeam=%d Score=%d:%d"),
		WinningTeam,
		GS->TeamScores.Num() > 0 ? GS->TeamScores[0] : 0,
		GS->TeamScores.Num() > 1 ? GS->TeamScores[1] : 0);

	//经济结算（简化：胜方 +3000，败方 +1900）
	AwardRoundEconomy(WinningTeam);

	EnterRoundState(ERoundState::RoundEnd);
}

void AShooterGameModeBase::StartNewRound()
{
	if (!HasAuthority()) return;

	AShooterGameStateBase* GS = GetShooterGameState();
	if (!GS) return;

	GS->RoundNumber++;

	//复活所有玩家
	RespawnAllPlayers();

	//S4-4：重置所有玩家的购买（移除购买的技能 + 武器重发默认），下回合重新买
	for (TActorIterator<AShooterCharacter> It(GetWorld()); It; ++It)
	{
		if (AShooterCharacter* Character = *It)
		{
			Character->ResetPurchases();
		}
	}

	EnterRoundState(ERoundState::Buy);
}

void AShooterGameModeBase::RespawnAllPlayers()
{
	if (!HasAuthority()) return;

	for (TActorIterator<AController> It(GetWorld()); It; ++It)
	{
		AController* Controller = *It;
		if (!IsValid(Controller)) continue;

		APlayerController* PC = Cast<APlayerController>(Controller);
		if (!PC) continue;

		AShooterPlayerState* PS = PC->GetPlayerState<AShooterPlayerState>();
		if (!PS) continue;

		//复活到队伍出生点
		AActor* Start = FindPlayerStartForTeam(PC, PS->TeamID);
		if (Start)
		{
			RestartPlayerAtPlayerStart(PC, Start);
		}
		else
		{
			RestartPlayer(PC);
		}
	}
}

void AShooterGameModeBase::AwardRoundEconomy(int32 WinningTeam)
{
	if (!HasAuthority()) return;

	for (TActorIterator<APlayerState> It(GetWorld()); It; ++It)
	{
		AShooterPlayerState* PS = Cast<AShooterPlayerState>(*It);
		if (!PS) continue;

		if (PS->TeamID == WinningTeam)
		{
			PS->Money += 3000;
		}
		else
		{
			PS->Money += 1900; // 败方补偿
		}
	}
}

AActor* AShooterGameModeBase::FindPlayerStartForTeam(AController* Player, int32 Team) const
{
	if (!IsValid(Player)) return nullptr;

	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);

	//按 PlayerStart 的 PlayerStartTag 区分队伍：Tag "Team1"/"Team2"
	//灰盒阶段：没有队伍 Tag 时退回随机出生点
	TArray<AActor*> TeamStarts;
	for (AActor* Start : PlayerStarts)
	{
		APlayerStart* PS = Cast<APlayerStart>(Start);
		if (PS && PS->PlayerStartTag == FName(*FString::Printf(TEXT("Team%d"), Team)))
		{
			TeamStarts.Add(Start);
		}
	}

	if (TeamStarts.Num() == 0)
	{
		//没有队伍出生点：退回全部随机
		TeamStarts = PlayerStarts;
	}

	if (TeamStarts.Num() == 0) return nullptr;

	const int32 Selection = FMath::RandRange(0, TeamStarts.Num() - 1);
	return TeamStarts[Selection];
}

AShooterGameStateBase* AShooterGameModeBase::GetShooterGameState() const
{
	return GetGameState<AShooterGameStateBase>();
}

void AShooterGameModeBase::RequestRespawn(ACharacter* Character, AController* Controller)
{
	/*
	 * 回合制语义（S4-6）：回合内不复活
	 * 这个函数现在只在「回合结束统一复活」时由 RespawnAllPlayers 走 RestartPlayer，不再即时调用
	 * 保留这个入口是为了兼容旧调用点（蓝图/其它组件），但在 RoundActive 期间直接忽略
	 */
	if (!IsValid(Character) || !IsValid(Controller)) return;

	AShooterGameStateBase* GS = GetShooterGameState();
	if (GS && GS->RoundState == ERoundState::RoundActive)
	{
		//回合进行中不复活
		return;
	}

	Character->Reset();
	Character->Destroy();
	RestartPlayer(Controller);
}

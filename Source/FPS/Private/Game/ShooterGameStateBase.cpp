// Copyright Druid Mechanics


#include "Game/ShooterGameStateBase.h"

#include "Net/UnrealNetwork.h"
#include "Player/ShooterPlayerState.h"

AShooterGameStateBase::AShooterGameStateBase()
{
	bHasFirstBloodBeenHad = false;

	RoundState = ERoundState::Warmup;
	RoundTime = 0.f;
	RoundNumber = 1;
	TeamScores.SetNum(2);
	TeamScores[0] = 0;
	TeamScores[1] = 0;
}

void AShooterGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterGameStateBase, RoundState);
	DOREPLIFETIME(AShooterGameStateBase, RoundTime);
	DOREPLIFETIME(AShooterGameStateBase, RoundNumber);
	DOREPLIFETIME(AShooterGameStateBase, TeamScores);
}

bool AShooterGameStateBase::HasFirstBloodBeenHad() const
{
	return bHasFirstBloodBeenHad;
}

void AShooterGameStateBase::UpdateLeader()
{
	TArray<APlayerState*> LocalSortedPlayers = PlayerArray;
	LocalSortedPlayers.Sort([](const APlayerState& A, const APlayerState& B)
	{
		const AShooterPlayerState* PlayerA = Cast<AShooterPlayerState>(&A);
		const AShooterPlayerState* PlayerB = Cast<AShooterPlayerState>(&B);
		return PlayerA->GetScoredElims() > PlayerB->GetScoredElims();
	});
	
	Leaders.Empty();
	
	if (LocalSortedPlayers.Num() > 0)
	{
		int32 HighestScore = 0;
		for (APlayerState* Player : LocalSortedPlayers)
		{
			AShooterPlayerState* ShooterPS = Cast<AShooterPlayerState>(Player);
			if (IsValid(ShooterPS))
			{
				int32 PlayerScore = ShooterPS->GetScoredElims();
			
				// on the first iteration, set the highest score
				if (Leaders.Num() == 0)
				{
					HighestScore = PlayerScore;
					Leaders.Add(ShooterPS);
				}
				else if (PlayerScore == HighestScore)
				{
					Leaders.Add(ShooterPS); // Add to Leaders if scores are tied
				}
				else
				{
					break; // As it's sorted, no need to check further
				}
			}
		}
	}
	
	bHasFirstBloodBeenHad = true;
}

AShooterPlayerState* AShooterGameStateBase::GetSoleLeader() const
{
	if (Leaders.Num() == 1)
	{
		return Leaders[0];
	}
	return nullptr;
}

bool AShooterGameStateBase::IsTiedForTheLead(AShooterPlayerState* PlayerState)
{
	return Leaders.Contains(PlayerState) && Leaders.Num() > 1;
}

void AShooterGameStateBase::ServerSetRoundState(ERoundState NewState)
{
	if (!HasAuthority()) return;
	RoundState = NewState;
	//标记复制，让客户端 OnRep_RoundState 触发
	ForceNetUpdate();
	//Listen Server 本地也广播（纯客户端走 OnRep_RoundState）
	OnRoundStateChanged.Broadcast(NewState);
}

void AShooterGameStateBase::ServerSetRoundTime(float NewTime)
{
	if (!HasAuthority()) return;
	RoundTime = NewTime;
}

void AShooterGameStateBase::OnRep_RoundState()
{
	//客户端回合阶段变化：蓝图 HUD 监听这个回调做切换（购买菜单/倒计时等）
	OnRoundStateChanged.Broadcast(RoundState);
}

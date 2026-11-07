// Copyright Aisssky


#include "Networking/NetworkTimeSubsystem.h"

#include "Engine/World.h"

UNetworkTimeSubsystem::UNetworkTimeSubsystem()
{
	MaxRewindSeconds = 0.4f;
	FutureToleranceSeconds = 0.05f;
	bClampRewindInsteadOfReject = true;
	ClientInterpolationDelaySeconds = 0.f;
	MaxSamples = 12;
	MaxSampleRoundTripSeconds = 1.f;

	EstimatedOffset = 0.0;
	EstimatedRoundTripTime = 0.0;
	bHasTimeSync = false;
}

bool UNetworkTimeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	//只在游戏世界创建（编辑器预览世界不需要）
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UNetworkTimeSubsystem* UNetworkTimeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UNetworkTimeSubsystem>() : nullptr;
}

bool UNetworkTimeSubsystem::IsServerOrStandalone() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

double UNetworkTimeSubsystem::GetLocalTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : 0.0;
}

double UNetworkTimeSubsystem::GetEstimatedServerTime() const
{
	//服务器自己就是时间基准
	if (IsServerOrStandalone())
	{
		return GetLocalTime();
	}

	if (bHasTimeSync)
	{
		return GetLocalTime() + EstimatedOffset;
	}

	//还没采到样本：0 = 未知。不能用 GameState::GetServerWorldTimeSeconds 兜底，那是世界时间轴，和这里的真实时间轴不是一回事
	return 0.0;
}

double UNetworkTimeSubsystem::GetEstimatedViewedServerTime() const
{
	if (IsServerOrStandalone())
	{
		return GetLocalTime();
	}
	//未同步：返回 0，服务器会降级为不回溯（按收包时刻判定）
	if (!bHasTimeSync)
	{
		return 0.0;
	}
	//客户端看到的其他玩家 = 服务器 RTT/2 之前的状态（再加插值延迟）
	return GetEstimatedServerTime() - EstimatedRoundTripTime * 0.5 - ClientInterpolationDelaySeconds;
}

float UNetworkTimeSubsystem::GetEstimatedErrorMs() const
{
	return static_cast<float>(EstimatedRoundTripTime * 0.5 * 1000.0);
}

void UNetworkTimeSubsystem::RecordRoundTripSample(double ClientSendTime, double ServerTime, double ClientReceiveTime)
{
	const double RoundTripTime = ClientReceiveTime - ClientSendTime;

	//过滤异常样本：负数（乱序/时钟错）或 RTT 过大
	if (RoundTripTime < 0.0 || RoundTripTime > MaxSampleRoundTripSeconds)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[NetTime] Drop sample RTT=%.1fms"), RoundTripTime * 1000.0);
		return;
	}

	/*
	 * 假设上下行延迟对称：服务器回包时刻 ServerTime 对应客户端时间 ≈ ClientSendTime + RTT/2
	 * 所以 offset = ServerTime - (ClientSendTime + RTT/2)
	 * 不能用「服务器收到时间 - Ping」当可靠开枪时刻（§13.3），这里只估算时钟偏移
	 */
	FTimeSyncSample Sample;
	Sample.RoundTripTime = RoundTripTime;
	Sample.Offset = ServerTime - (ClientSendTime + RoundTripTime * 0.5);

	Samples.Add(Sample);
	if (Samples.Num() > MaxSamples)
	{
		Samples.RemoveAt(0);
	}

	RecomputeEstimate();

	UE_LOG(LogTemp, Log, TEXT("[NetTime] RTT=%.1fms Offset=%.4fs EstRTT=%.1fms Err=%.1fms Samples=%d"),
		RoundTripTime * 1000.0, Sample.Offset, EstimatedRoundTripTime * 1000.0, GetEstimatedErrorMs(), Samples.Num());
}

void UNetworkTimeSubsystem::RecomputeEstimate()
{
	if (Samples.Num() == 0) return;

	TArray<FTimeSyncSample> Sorted = Samples;
	Sorted.Sort([](const FTimeSyncSample& A, const FTimeSyncSample& B)
	{
		return A.RoundTripTime < B.RoundTripTime;
	});

	//只用 RTT 最小的一半：排队/抖动越少的样本，对称假设越成立
	const int32 UseCount = FMath::Max(1, Sorted.Num() / 2);
	double OffsetSum = 0.0;
	double RttSum = 0.0;
	for (int32 i = 0; i < UseCount; ++i)
	{
		OffsetSum += Sorted[i].Offset;
		RttSum += Sorted[i].RoundTripTime;
	}

	EstimatedOffset = OffsetSum / UseCount;
	EstimatedRoundTripTime = RttSum / UseCount;
	bHasTimeSync = true;
}

bool UNetworkTimeSubsystem::ConvertClientShotTimeToServerTime(double ClientShotTime, double& OutServerShotTime, float& OutEstimatedErrorMs) const
{
	//约定：客户端已经换算到服务器时间轴（见顶部协议第 3 条），这里只拦非法值
	if (!FMath::IsFinite(ClientShotTime) || ClientShotTime <= 0.0)
	{
		return false;
	}

	OutServerShotTime = ClientShotTime;
	//服务器不知道客户端的样本，误差上限按允许的超前容差给出
	OutEstimatedErrorMs = FutureToleranceSeconds * 1000.f;
	return true;
}

bool UNetworkTimeSubsystem::IsShotTimeWithinAllowedWindow(double& InOutServerShotTime, double ServerReceiveTime, EShotRejectReason& OutRejectReason) const
{
	OutRejectReason = EShotRejectReason::None;

	//来自未来：客户端不可能看到服务器还没发生的事
	if (InOutServerShotTime > ServerReceiveTime + FutureToleranceSeconds)
	{
		OutRejectReason = EShotRejectReason::TimestampInFuture;
		return false;
	}

	//未来容差内的小幅超前，按「现在」处理
	InOutServerShotTime = FMath::Min(InOutServerShotTime, ServerReceiveTime);

	const double RewindAge = ServerReceiveTime - InOutServerShotTime;
	if (RewindAge > MaxRewindSeconds)
	{
		if (!bClampRewindInsteadOfReject)
		{
			OutRejectReason = EShotRejectReason::OutsideRewindWindow;
			return false;
		}

		//降级：钳到窗口边界，记录原因但不拒绝
		UE_LOG(LogTemp, Log, TEXT("[NetTime] Rewind clamped Age=%.1fms Max=%.1fms"), RewindAge * 1000.0, MaxRewindSeconds * 1000.0);
		InOutServerShotTime = ServerReceiveTime - MaxRewindSeconds;
	}
	return true;
}

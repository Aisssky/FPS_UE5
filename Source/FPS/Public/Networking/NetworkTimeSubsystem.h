// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Networking/NetworkValidationTypes.h"
#include "NetworkTimeSubsystem.generated.h"

/*
 * 时间协议（全项目统一，改之前先看这里）：
 * 1.单位：秒（double）。日志里的 Ms 字段才是毫秒。
 * 2.时间轴：一律用 UWorld::GetRealTimeSeconds()（真实时间，不受时间膨胀/暂停/帧时间钳制影响）。
 *   - 服务器：直接就是「服务器时间」，历史帧（LagCompensation）、收包时刻也按它记录。
 *   - 客户端：本地真实时间 + EstimatedOffset = 估算的服务器时间。
 *   - 踩坑：最早用的是 GetTimeSeconds()（游戏时间），DS+客户端实测两边走速不一致，
 *     offset 每 7 秒漂移约 0.5 秒，时间同步完全失效，所以改成真实时间。
 *   - 客户端未完成同步时 ClientShotTime=0（未知），服务器降级为不回溯。
 * 3.FShotRequest::ClientShotTime 约定为「客户端开枪那一刻看到的画面所对应的服务器时间」
 *   = 估算服务器当前时间 - 单程延迟(RTT/2) - 插值延迟。
 *   因为客户端看到的其他玩家位置是 RTT/2 之前服务器复制过来的。
 * 4.客户端时间只是一条待验证的数据，不是信任凭据：服务器必须做窗口校验。
 *
 * 归属：UWorldSubsystem，每个 World 独立一份（PIE 多客户端互不干扰），换地图自动重建。
 * 采样：由 AShooterPlayerController 定时发 RPC 往返，结果喂给 RecordRoundTripSample。
 */

//一次往返采样
struct FTimeSyncSample
{
	double RoundTripTime=0.0;	//RTT（秒）
	double Offset=0.0;			//服务器时间 - 客户端本地时间（秒）
};

UCLASS(Config=Game)
class FPS_API UNetworkTimeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	UNetworkTimeSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	static UNetworkTimeSubsystem* Get(const UObject* WorldContextObject);

	/** 客户端/服务器通用 */

	//本地世界时间（秒）
	double GetLocalTime() const;

	//估算的服务器当前时间；服务器/单机直接返回本地时间
	double GetEstimatedServerTime() const;

	//客户端「看到的画面」对应的服务器时间，填 ClientShotTime 用（见顶部协议第 3 条）
	double GetEstimatedViewedServerTime() const;

	//估算误差（毫秒），约等于最佳样本 RTT 的一半
	float GetEstimatedErrorMs() const;

	double GetEstimatedRoundTripTime() const { return EstimatedRoundTripTime; }
	bool HasTimeSync() const { return bHasTimeSync; }

	/** 客户端：喂入一次往返采样 */

	//ClientSendTime/ClientReceiveTime 是客户端本地时间，ServerTime 是服务器回包时刻
	void RecordRoundTripSample(double ClientSendTime, double ServerTime, double ClientReceiveTime);

	/** 服务器：校验客户端时间戳 */

	//ClientShotTime 已经是客户端估算的服务器时间轴，这里只做有效性检查并给出误差估计
	bool ConvertClientShotTimeToServerTime(double ClientShotTime, double& OutServerShotTime, float& OutEstimatedErrorMs) const;

	//未来/超窗口检查；bClampRewindInsteadOfReject=true 时超窗口会被钳到窗口边界（降级，不拒绝）
	bool IsShotTimeWithinAllowedWindow(double& InOutServerShotTime, double ServerReceiveTime, EShotRejectReason& OutRejectReason) const;

	//允许回溯的最大时长（秒），HitValidation/LagCompensation 共用
	UPROPERTY(Config, EditAnywhere, Category = "FPS|NetworkTime")
	float MaxRewindSeconds;

	//允许时间戳比服务器当前时间超前多少（秒），吸收时钟估计误差
	UPROPERTY(Config, EditAnywhere, Category = "FPS|NetworkTime")
	float FutureToleranceSeconds;

	//超过回溯窗口时：true=钳到窗口边界继续处理（高延迟玩家仍可命中，但不再回溯到真实时刻）；false=直接拒绝
	UPROPERTY(Config, EditAnywhere, Category = "FPS|NetworkTime")
	bool bClampRewindInsteadOfReject;

	//客户端额外插值延迟（秒），模拟代理平滑带来的显示滞后；默认 0
	UPROPERTY(Config, EditAnywhere, Category = "FPS|NetworkTime")
	float ClientInterpolationDelaySeconds;

	//最多保留多少个样本
	UPROPERTY(Config, EditAnywhere, Category = "FPS|NetworkTime")
	int32 MaxSamples;

	//RTT 超过这个值的样本直接丢掉（异常样本）
	UPROPERTY(Config, EditAnywhere, Category = "FPS|NetworkTime")
	float MaxSampleRoundTripSeconds;

private:

	//重新计算 offset/RTT：取 RTT 最小的一半样本求平均（RTT 越小，offset 越可信）
	void RecomputeEstimate();

	bool IsServerOrStandalone() const;

	TArray<FTimeSyncSample> Samples;
	double EstimatedOffset;
	double EstimatedRoundTripTime;
	bool bHasTimeSync;
};

// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "NetworkValidationTypes.generated.h"

/*
 * 射击请求被服务器拒绝的原因（结构化，见主文档 §17.2）
 * 不只写进临时日志：HUD、自动化测试、演示都读这个枚举
 */
UENUM(BlueprintType)
enum class EShotRejectReason : uint8
{
	None,						//未拒绝
	InvalidShooter,				//射手无效/已死亡/拥有权不对
	InvalidWeapon,				//没有装备武器
	NoAmmo,						//弹匣为空
	FireRateExceeded,			//超过射速
	AbilityBlocked,				//状态不允许（换弹中等）
	DuplicateSequence,			//ShotSequence 重复/过旧
	InvalidDirection,			//方向为零/NaN/未归一化
	OriginOutOfTolerance,		//射线起点与服务器认定位置偏差过大
	TimestampInFuture,			//时间戳比服务器当前时间还晚
	TimestampTooOld,			//时间戳比上一发已接受的射击还早（时间倒流）
	OutsideRewindWindow,		//超出最大回溯窗口
	TargetNotValidAtShotTime	//目标在射击时刻没有可用历史数据
};

namespace FPSNetValidation
{
	//枚举转字符串，日志用
	inline FString RejectReasonToString(EShotRejectReason Reason)
	{
		return StaticEnum<EShotRejectReason>()->GetNameStringByValue(static_cast<int64>(Reason));
	}
}

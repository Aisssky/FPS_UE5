// Copyright Aisssky

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Networking/HistoricalHitboxTypes.h"
#include "LagCompensationComponent.generated.h"

/*
 * 历史帧缓冲（M5）
 * 职责：只在服务器上记录/查询本角色的历史 hitbox。
 * 不改 Health、不播 UI、不做 trace（trace 在 HitValidationComponent）。
 * 每个 Character 自己保存历史；死亡时清空，重生是新 Character = 新历史段。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULagCompensationComponent();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintPure, Category = "FPS|LagCompensation")
	static ULagCompensationComponent* FindLagCompensationComponent(const AActor* Actor) { return IsValid(Actor) ? Actor->FindComponentByClass<ULagCompensationComponent>() : nullptr; }
	
	//记录当前时刻一帧
	void RecordHistoricalFrame();
	
	//删掉超出 MaxRecordTimeSeconds 的旧帧
	void TrimHistory();
	
	//死亡/销毁时清空，开始新的历史段
	void ClearHistory();
	
	//查询指定服务器时刻：早于最老帧/没有数据 -> false；落在两帧之间 -> 插值
	bool GetHistoricalFrameAtTime(double ServerTime, FHistoricalFrame& OutFrame) const;
	
	//在 [TimeBefore, TimeAfter] 两帧之间插值；hitbox 数量/名字对不上返回 false，不静默拼错数据
	bool InterpolateHistoricalFrame(double ServerTime, FHistoricalFrame& OutFrame) const;
	
	//开发调试：画出某一帧（历史=黄色，当前=绿色）
	void DrawHistoricalHitboxes(const FHistoricalFrame& Frame, const FColor& Color, float Duration) const;
	
	//构造当前时刻的帧（不写入缓冲），调试对比用
	void BuildFrameNow(FHistoricalFrame& OutFrame) const;
	
	int32 GetNumFrames() const { return History.Num(); }
	
	//记录多长时间的历史（秒）；应 >= NetworkTime 的 MaxRewindSeconds
	UPROPERTY(EditDefaultsOnly, Category = "FPS|LagCompensation")
	float MaxRecordTimeSeconds;
	
	//第一版只做 head/torso/pelvis，不记录整条骨骼链
	UPROPERTY(EditDefaultsOnly, Category = "FPS|LagCompensation")
	TArray<FHitboxDefinition> HitboxDefinitions;
	
protected:
	virtual void BeginPlay() override;
	
private:
	
	//按时间从旧到新排列；帧数很少（~30 帧/秒 * 1 秒），按时间清理的数组足够，M8 再评估环形缓冲
	TArray<FHistoricalFrame> History;
	
	bool ShouldRecord() const;
	
	static void LerpFrame(const FHistoricalFrame& Older, const FHistoricalFrame& Newer, double Alpha, FHistoricalFrame& OutFrame);
};

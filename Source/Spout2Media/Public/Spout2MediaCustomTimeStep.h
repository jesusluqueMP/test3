// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenlockedCustomTimeStep.h"
#include "GenlockedFixedRateCustomTimeStep.h"
#include "MediaPlayer.h"
#include "Spout2MediaPlayer.h"
#include "Spout2MediaCustomTimeStep.generated.h"

UCLASS(Blueprintable)
class SPOUT2MEDIA_API USpout2MediaCustomTimeStep : public UGenlockedFixedRateCustomTimeStep
{
	GENERATED_UCLASS_BODY()

	TSharedPtr<FSpout2MediaPlayer, ESPMode::ThreadSafe> SpoutMediaPlayer;
	
public:
	//~ UFixedFrameRateCustomTimeStep interface
	virtual bool Initialize(UEngine* InEngine) override;
	virtual void Shutdown(UEngine* InEngine) override;
	virtual bool UpdateTimeStep(UEngine* InEngine) override;
	virtual ECustomTimeStepSynchronizationState GetSynchronizationState() const override;
	virtual FFrameRate GetFixedFrameRate() const override;

	//~ UGenlockedCustomTimeStep interface
	virtual uint32 GetLastSyncCountDelta() const override;
	virtual bool IsLastSyncDataValid() const override;
	virtual bool WaitForSync() override;

public:
	// Media player to synchronize with
	UPROPERTY(EditAnywhere, Category="Spout Media")
	TSoftObjectPtr<UMediaPlayer> MediaPlayer;
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FixedFrameRateCustomTimeStep.h"
#include "Spout2MediaCapture.h"
#include "Spout2MediaOutput.h"
#include "Spout2MediaOutputCustomTimeStep.generated.h"

/**
 * Custom time step class for controlling engine frame rate when sending via Spout
 */
UCLASS(Blueprintable)
class SPOUT2MEDIA_API USpout2MediaOutputCustomTimeStep : public UFixedFrameRateCustomTimeStep
{
	GENERATED_UCLASS_BODY()

public:
	//~ UFixedFrameRateCustomTimeStep interface
	virtual bool Initialize(UEngine* InEngine) override;
	virtual void Shutdown(UEngine* InEngine) override;
	virtual bool UpdateTimeStep(UEngine* InEngine) override;
	virtual FFrameRate GetFixedFrameRate() const override;

public:
	// Media output to synchronize with
	UPROPERTY(EditAnywhere, Category="Spout Media")
	TSoftObjectPtr<USpout2MediaOutput> MediaOutput;
	
	// Media capture associated with the output
	UPROPERTY(Transient)
	USpout2MediaCapture* MediaCapture;
	
	// Start capture automatically on initialization
	UPROPERTY(EditAnywhere, Category="Spout Media")
	bool bAutoStartCapture = true;
	
	// Target to capture from
	UPROPERTY(EditAnywhere, Category="Spout Media", meta=(EditCondition="bAutoStartCapture"))
	TSoftObjectPtr<UTextureRenderTarget2D> TargetRenderTarget;
};
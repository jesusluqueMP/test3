// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseMediaSource.h"

#include "Spout2MediaSource.generated.h"

UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Spout2 Media Source"), HideCategories=("Platforms"))
class SPOUT2MEDIA_API USpout2MediaSource
	: public UBaseMediaSource
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media")
	FString SourceName = FString("Spout");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media")
	bool bSRGB = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media")
	FFrameRate TargetFrameRate = FFrameRate(60, 1);
	
	// Whether to use frame synchronization for precise frame timing
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media|Synchronization")
	bool bUseFrameSync = false;
	
	// Maximum time to wait for frame sync in milliseconds (0 = don't wait)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media|Synchronization", meta=(EditCondition="bUseFrameSync", ClampMin="0", ClampMax="1000"))
	int32 FrameSyncTimeoutMs = 100;
	
	// Whether to link the engine's rendering to the Spout frame sync
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media|Synchronization")
	bool bLinkRenderingToFrameSync = false;

	virtual bool Validate() const override { return true; }
	virtual FString GetUrl() const override;
};
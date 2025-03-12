// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MediaOutput.h"
#include "MediaIOCoreDefinitions.h"

#include "Spout2MediaOutput.generated.h"

UCLASS(BlueprintType, meta=(DisplayName="Spout2 Media Output"))
class SPOUT2MEDIA_API USpout2MediaOutput
	: public UMediaOutput
{
	GENERATED_UCLASS_BODY()
public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media")
	FString SenderName = FString("UnrealEngine");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media")
	FIntPoint OutputSize = FIntPoint(1920, 1080);
	
	// The desired frame rate for the Spout output
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media")
	FFrameRate OutputFrameRate = FFrameRate(60, 1);
	
	// Whether to link Spout frame syncs directly to the render thread
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media|Synchronization")
	bool bLinkToRenderThread = true;
	
	// Enable/disable frame rate control
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spout2 Media|Synchronization")
	bool bEnableFrameRateControl = true;
	
	// Gets sender name with embedded frame rate information
	UFUNCTION(BlueprintCallable, Category = "Spout2 Media")
	FString GetModifiedSenderName() const;
	
	virtual bool Validate(FString& OutFailureReason) const override;

	virtual FIntPoint GetRequestedSize() const override;
	virtual EPixelFormat GetRequestedPixelFormat() const override;
	virtual EMediaCaptureConversionOperation
	GetConversionOperation(EMediaCaptureSourceType InSourceType) const override;

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() override;
};
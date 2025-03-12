// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MediaCapture.h"
#include "Spout2MediaOutput.h"

#include "Spout2MediaCapture.generated.h"

class FSpoutFrameSyncHelper;

UCLASS(BlueprintType)
class SPOUT2MEDIA_API USpout2MediaCapture
	: public UMediaCapture
{
	GENERATED_UCLASS_BODY()

	struct FSpoutSenderContext;
	TSharedPtr<FSpoutSenderContext> Context;

public:
	virtual bool HasFinishedProcessing() const override;
	
	// Frame synchronization controls
	UFUNCTION(BlueprintCallable, Category = "Spout2 Media|Synchronization")
	void SetFrameRate(int32 FramesPerSecond);
	
	UFUNCTION(BlueprintCallable, Category = "Spout2 Media|Synchronization")
	void DisableFrameRateControl();
	
	UFUNCTION(BlueprintCallable, Category = "Spout2 Media|Synchronization")
	bool IsFrameRateControlEnabled() const;
	
	UFUNCTION(BlueprintCallable, Category = "Spout2 Media|Synchronization")
	void SignalFrameSync();
	
	// Get the sender name
	UFUNCTION(BlueprintCallable, Category = "Spout2 Media")
	FString GetSenderName() const;
	
protected:
	virtual bool ValidateMediaOutput() const override;

	virtual bool InitializeCapture() override;
	
	virtual bool UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport) override;
	virtual bool UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget) override { return false; }
	
	virtual void StopCaptureImpl(bool bAllowPendingFrameToBeProcess) override;

	virtual bool ShouldCaptureRHIResource() const override { return true; }
	virtual void OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture) override;

private:
	// Store the output frame rate from Spout2MediaOutput
	FFrameRate OutputFrameRate;
	
	// Flag to indicate if render thread sync should be used
	bool bLinkToRenderThread;
	
	// Frame sync helper
	TSharedPtr<FSpoutFrameSyncHelper> FrameSyncHelper;

	bool InitSpout(USpout2MediaOutput* Output);
	bool DisposeSpout();
};
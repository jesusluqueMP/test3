// Fill out your copyright notice in the Description page of Project Settings.

#include "Spout2MediaOutputCustomTimeStep.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"

USpout2MediaOutputCustomTimeStep::USpout2MediaOutputCustomTimeStep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USpout2MediaOutputCustomTimeStep::Initialize(UEngine* InEngine)
{
	// Load the media output asset
	USpout2MediaOutput* Output = MediaOutput.LoadSynchronous();
	if (!Output)
	{
		return false;
	}
	
	// Start the capture if auto-start is enabled
	if (bAutoStartCapture)
	{
		MediaCapture = Cast<USpout2MediaCapture>(Output->CreateMediaCapture());
		if (MediaCapture)
		{
			// Handle the target based on the configuration
			if (TargetRenderTarget.IsValid())
			{
				UTextureRenderTarget2D* RenderTarget = TargetRenderTarget.LoadSynchronous();
				if (RenderTarget)
				{
					// Create capture options
					FMediaCaptureOptions Options;
					// Note: bResizeSourceToFit is not available in this version
					
					// Start capturing
					MediaCapture->CaptureTextureRenderTarget2D(RenderTarget, Options);
				}
			}
			else
			{
				// If no specific target, capture the active viewport
				FMediaCaptureOptions Options;
				// Note: bResizeSourceToFit is not available in this version
				
				// Start capturing
				MediaCapture->CaptureActiveSceneViewport(Options);
			}
		}
	}
	
	return true;
}

void USpout2MediaOutputCustomTimeStep::Shutdown(UEngine* InEngine)
{
	// Stop the capture when shutting down
	if (MediaCapture && MediaCapture->GetState() == EMediaCaptureState::Capturing)
	{
		MediaCapture->StopCapture(false);
	}
	
	MediaCapture = nullptr;
}

bool USpout2MediaOutputCustomTimeStep::UpdateTimeStep(UEngine* InEngine)
{
	// Standard fixed frame rate behavior - this controls the engine's frame rate
	return true;
}

FFrameRate USpout2MediaOutputCustomTimeStep::GetFixedFrameRate() const
{
	// Get the frame rate from the media output
	USpout2MediaOutput* Output = MediaOutput.LoadSynchronous();
	if (Output)
	{
		return Output->OutputFrameRate;
	}
	
	// Default to 60fps if output is not available
	return FFrameRate(60, 1);
}
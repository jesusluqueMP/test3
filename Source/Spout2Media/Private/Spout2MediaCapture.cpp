// Fill out your copyright notice in the Description page of Project Settings.

#include "Spout2MediaCapture.h"
#include "Spout2MediaOutput.h"
#include "SpoutFrameSyncHelper.h"
#include "RenderingThread.h"
#include "RHICommandList.h"

#include "Windows/AllowWindowsPlatformTypes.h" 
#include <d3d11on12.h>
#include "Spout.h"
#include "Windows/HideWindowsPlatformTypes.h"

struct USpout2MediaCapture::FSpoutSenderContext
{
	FString SenderName;
	uint32 Width, Height;
	EPixelFormat PixelFormat;

	std::string SenderName_str;

	ID3D11DeviceContext* DeviceContext = nullptr;
	ID3D11Device* D3D11Device = nullptr;
	
	ID3D11On12Device* D3D11on12Device = nullptr;
	TMap<FTextureRHIRef, ID3D11Resource*> WrappedDX11ResourceMap;

	spoutSenderNames senders;
	spoutDirectX sdx;

	ID3D11Texture2D* SendingTexture = nullptr;
	HANDLE SharedSendingHandle = nullptr;

	// Frame rate control variables
	FFrameRate TargetFrameRate;
	double LastFrameTime;
	double FrameInterval;  // Time between frames in seconds

	FSpoutSenderContext(const FString& SenderName,
		uint32 Width, uint32 Height, EPixelFormat PixelFormat,
		FTextureRHIRef InTexture)
		: SenderName(SenderName)
		, Width(Width)
		, Height(Height)
		, PixelFormat(PixelFormat)
		, LastFrameTime(0.0)
		, FrameInterval(1.0/60.0) // Default to 60fps
	{
		SenderName_str = TCHAR_TO_ANSI(*SenderName);
		InitSpout(InTexture);
	}

	~FSpoutSenderContext()
	{
		DisposeSpout();
	}

	void InitSpout(FTextureRHIRef InTexture)
	{
		const FString RHIName = GDynamicRHI->GetName();

		if (RHIName == TEXT("D3D11"))
		{
			D3D11Device = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
			D3D11Device->GetImmediateContext(&DeviceContext);
		}
		else if (RHIName == TEXT("D3D12"))
		{
			ID3D12Device* Device12 = static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice());
			UINT DeviceFlags11 = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

			verify(D3D11On12CreateDevice(
				Device12,
				DeviceFlags11,
				nullptr,
				0,
				nullptr,
				0,
				0,
				&D3D11Device,
				&DeviceContext,
				nullptr
			) == S_OK);

			verify(D3D11Device->QueryInterface(__uuidof(ID3D11On12Device), (void**)&D3D11on12Device) == S_OK);
		}

		ID3D12Resource* NativeTex = (ID3D12Resource*)InTexture->GetNativeResource();
		D3D12_RESOURCE_DESC desc = NativeTex->GetDesc();
		
		verify(senders.CreateSender(SenderName_str.c_str(), Width, Height, SharedSendingHandle, desc.Format));
		verify(sdx.CreateSharedDX11Texture(D3D11Device, Width, Height, desc.Format, &SendingTexture, SharedSendingHandle));
	}

	void DisposeSpout()
	{
		if (SendingTexture)
		{
			SendingTexture->Release();
			SendingTexture = nullptr;
		}

		if (DeviceContext)
		{
			DeviceContext->Release();
			DeviceContext = nullptr;
		}

		{
			for (auto Iter : WrappedDX11ResourceMap)
			{
				D3D11on12Device->ReleaseWrappedResources(&Iter.Value, 1);
			}
			WrappedDX11ResourceMap.Reset();
		}

		if (D3D11on12Device)
		{
			D3D11on12Device->Release();
			D3D11on12Device = nullptr;
		}

		if (D3D11Device)
		{
			D3D11Device->Release();
			D3D11Device = nullptr;
		}
	}

	ID3D11Texture2D* GetTextureResource(FTextureRHIRef InTexture)
	{
		const FString RHIName = GDynamicRHI->GetName();
		
		if (RHIName == TEXT("D3D11"))
		{
			return static_cast<ID3D11Texture2D*>(InTexture->GetNativeResource());
		}
		else if (RHIName == TEXT("D3D12"))
		{
			if (auto Iter = WrappedDX11ResourceMap.Find(InTexture))
				return static_cast<ID3D11Texture2D*>(*Iter);

			ID3D12Resource* NativeTex = static_cast<ID3D12Resource*>(InTexture->GetNativeResource());
			ID3D11Resource* WrappedDX11Resource = nullptr;
			
			D3D11_RESOURCE_FLAGS rf11 = {};
			verify(D3D11on12Device->CreateWrappedResource(
				NativeTex, &rf11,
				D3D12_RESOURCE_STATE_COPY_SOURCE,
				D3D12_RESOURCE_STATE_PRESENT, __uuidof(ID3D11Resource),
				(void**)&WrappedDX11Resource) == S_OK);

			NativeTex->Release();

			WrappedDX11ResourceMap.Add(InTexture, WrappedDX11Resource);

			return static_cast<ID3D11Texture2D*>(WrappedDX11Resource);
		}
		
		return nullptr;
	}

	void SetFrameRate(const FFrameRate& InFrameRate)
	{
		TargetFrameRate = InFrameRate;
		// Calculate frame interval in seconds
		FrameInterval = (double)TargetFrameRate.Denominator / (double)TargetFrameRate.Numerator;
	}
    
	bool ShouldSendFrame()
	{
		double CurrentTime = FPlatformTime::Seconds();
		bool bShouldSend = (CurrentTime - LastFrameTime) >= FrameInterval;
        
		if (bShouldSend)
		{
			LastFrameTime = CurrentTime;
		}
        
		return bShouldSend;
	}

	void Tick_RenderThread(FTextureRHIRef InTexture)
	{
		const FString RHIName = GDynamicRHI->GetName();

		if (!DeviceContext)
			return;

		// Only process the frame if it's time to send a new one
		if (!ShouldSendFrame())
			return;

		auto Texture = GetTextureResource(InTexture);
		DeviceContext->CopyResource(SendingTexture, Texture);
		DeviceContext->Flush();
		
		verify(senders.UpdateSender(SenderName_str.c_str(),
			Width, Height,
			SharedSendingHandle));
	}
};

// Fixed constructor to avoid initialization list issues
USpout2MediaCapture::USpout2MediaCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bLinkToRenderThread = true;
	FrameSyncHelper = MakeShared<FSpoutFrameSyncHelper>();
}

bool USpout2MediaCapture::HasFinishedProcessing() const
{
	return Super::HasFinishedProcessing();
}

void USpout2MediaCapture::SetFrameRate(int32 FramesPerSecond)
{
	if (FrameSyncHelper)
	{
		FrameSyncHelper->HoldFps(FramesPerSecond);
	}
}

void USpout2MediaCapture::DisableFrameRateControl()
{
	if (FrameSyncHelper)
	{
		FrameSyncHelper->DisableFrameCount();
	}
}

bool USpout2MediaCapture::IsFrameRateControlEnabled() const
{
	if (FrameSyncHelper)
	{
		return FrameSyncHelper->IsFrameCountEnabled();
	}
	return false;
}

void USpout2MediaCapture::SignalFrameSync()
{
	if (FrameSyncHelper)
	{
		USpout2MediaOutput* Output = CastChecked<USpout2MediaOutput>(MediaOutput);
		if (Output)
		{
			FrameSyncHelper->SetFrameSync(Output->SenderName);
		}
	}
}

FString USpout2MediaCapture::GetSenderName() const
{
	if (MediaOutput)
	{
		USpout2MediaOutput* Output = CastChecked<USpout2MediaOutput>(MediaOutput);
		if (Output)
		{
			return Output->SenderName;
		}
	}
	return TEXT("");
}

bool USpout2MediaCapture::ValidateMediaOutput() const
{
	USpout2MediaOutput* Output = CastChecked<USpout2MediaOutput>(MediaOutput);
	check(Output);
	return true;
}

bool USpout2MediaCapture::InitializeCapture()
{
	USpout2MediaOutput* Output = CastChecked<USpout2MediaOutput>(MediaOutput);
	check(Output);
	return InitSpout(Output);
}

bool USpout2MediaCapture::UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	return true;
}

void USpout2MediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	Super::StopCaptureImpl(bAllowPendingFrameToBeProcess);
	DisposeSpout();
}

void USpout2MediaCapture::OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData,
	TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture)
{
	USpout2MediaOutput* Output = CastChecked<USpout2MediaOutput>(MediaOutput);
	
	// Use the modified sender name with embedded frame rate info
	const FString SenderName = Output->GetModifiedSenderName();
	auto InTexture2D = InTexture->GetTexture2D();
	uint32 Width = InTexture2D->GetSizeX();
	uint32 Height = InTexture2D->GetSizeY();
	EPixelFormat PixelFormat = InTexture2D->GetFormat();

	if (!Context
		|| Context->SenderName != SenderName
		|| Context->Width != Width
		|| Context->Height != Height
		|| Context->PixelFormat != PixelFormat)
	{
		if (Context)
			Context.Reset();
		
		Context = MakeShared<FSpoutSenderContext, ESPMode::ThreadSafe>(
			SenderName, Width, Height, PixelFormat, InTexture);
		
		// Set the frame rate on initialization
		Context->SetFrameRate(OutputFrameRate);
	}

	if (Context)
	{
		Context->Tick_RenderThread(InTexture);
		
		// Signal frame sync after sending the frame - this is the key part that links
		// Unreal's rendering with the Spout sync
		if (FrameSyncHelper)
		{
			// We're already in the render thread so this is synchronized with the frame render
			FrameSyncHelper->SetFrameSync(Output->SenderName);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

bool USpout2MediaCapture::InitSpout(USpout2MediaOutput* Output)
{
	// Store the output frame rate from media output
	OutputFrameRate = Output->OutputFrameRate;
	
	// Get the link to render thread setting
	bLinkToRenderThread = Output->bLinkToRenderThread;
	
	// Configure frame rate control
	if (FrameSyncHelper)
	{
		FrameSyncHelper->HoldFps(OutputFrameRate.Numerator / OutputFrameRate.Denominator);
	}
	
	SetState(EMediaCaptureState::Capturing);
	return true;
}

bool USpout2MediaCapture::DisposeSpout()
{
	if (FrameSyncHelper && MediaOutput)
	{
		USpout2MediaOutput* Output = CastChecked<USpout2MediaOutput>(MediaOutput);
		if (Output)
		{
			FrameSyncHelper->ClearFrameSync(Output->SenderName);
		}
	}
	
	SetState(EMediaCaptureState::Stopped);
	Context.Reset();
	return true;
}
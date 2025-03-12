// Fill out your copyright notice in the Description page of Project Settings.


#include "Spout2MediaPlayer.h"

#include "Windows/AllowWindowsPlatformTypes.h" 
#include <d3d11on12.h>
#include "Spout.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "RHICommandList.h"
#include "MediaShaders.h"
#include "RenderingThread.h"

#include "Spout2MediaTextureSample.h"
#include "Spout2MediaSource.h"
#include "SpoutFrameSyncHelper.h"

static spoutSenderNames senders;

/////////////////////////////////////////////////////////////////////////////

struct FSpout2MediaPlayer::FSpoutReceiverContext
{
	unsigned int Width = 0, Height = 0;
	DXGI_FORMAT DXFormat = DXGI_FORMAT_UNKNOWN;
	EPixelFormat PixelFormat = PF_Unknown;

	ID3D11DeviceContext* Context = nullptr;
	
	ID3D11Device* D3D11Device = nullptr;
	ID3D12Device* D3D12Device = nullptr;
	ID3D11On12Device* D3D11on12Device = nullptr;

	FSpoutReceiverContext(unsigned int Width, unsigned int Height, DXGI_FORMAT DXFormat)
		: Width(Width)
		, Height(Height)
		, DXFormat(DXFormat)
	{
		if (DXFormat == DXGI_FORMAT_B8G8R8A8_UNORM)
			PixelFormat = PF_B8G8R8A8;
		else if (DXFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
			PixelFormat = PF_FloatRGBA;
		else if (DXFormat == DXGI_FORMAT_R32G32B32A32_FLOAT)
			PixelFormat = PF_A32B32G32R32F;

		FString RHIName = GDynamicRHI->GetName();

		if (RHIName == TEXT("D3D11"))
		{
			D3D11Device = (ID3D11Device*)GDynamicRHI->RHIGetNativeDevice();
			D3D11Device->GetImmediateContext(&Context);
		}
		else if (RHIName == TEXT("D3D12"))
		{
			D3D12Device = static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice());
			UINT DeviceFlags11 = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

			verify(D3D11On12CreateDevice(
				D3D12Device,
				DeviceFlags11,
				nullptr,
				0,
				nullptr,
				0,
				0,
				&D3D11Device,
				&Context,
				nullptr
			) == S_OK);

			verify(D3D11Device->QueryInterface(__uuidof(ID3D11On12Device), reinterpret_cast<void**>(&D3D11on12Device)) == S_OK);
		}
		else throw;
	}

	~FSpoutReceiverContext()
	{
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

		if (D3D12Device)
		{
			D3D12Device->Release();
			D3D12Device = nullptr;
		}

		if (Context)
		{
			Context->Release();
			Context = nullptr;
		}

	}
};

//////////////////////////////////////////////////////////////////////////

FSpout2MediaPlayer::FSpout2MediaPlayer(IMediaEventSink& InEventSink)
{
    bUseFrameSync = false;
    bLinkRenderingToFrameSync = false;
    FrameSyncHelper = MakeShared<FSpoutFrameSyncHelper>();
}

FSpout2MediaPlayer::~FSpout2MediaPlayer()
{
	// If we have a render thread delegate, remove it
	if (PreRenderDelegateHandle.IsValid())
	{
		FRenderCommandFence Fence;
		ENQUEUE_RENDER_COMMAND(RemovePreRenderDelegate)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			// Note: OnPreRender doesn't exist in this version of UE
			// Using a different approach without delegates
		});
		Fence.Wait();
	}
	
	Context.Reset();
}

void FSpout2MediaPlayer::Close()
{
	if (FrameSyncHelper && !SubscribeName.IsNone())
	{
		FrameSyncHelper->ClearFrameSync(SubscribeName.ToString());
	}
	
	// If we have a render thread delegate, remove it
	if (PreRenderDelegateHandle.IsValid())
	{
		FRenderCommandFence Fence;
		ENQUEUE_RENDER_COMMAND(RemovePreRenderDelegate)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			// Note: OnPreRender doesn't exist in this version of UE
			// Using a different approach without delegates
		});
		Fence.Wait();
		PreRenderDelegateHandle.Reset();
	}
	
	CurrentState = EMediaState::Closed;
	Context.Reset();
}

IMediaCache& FSpout2MediaPlayer::GetCache()
{
	return *this;
}

IMediaControls& FSpout2MediaPlayer::GetControls()
{
	return *this;
}

FString FSpout2MediaPlayer::GetInfo() const
{
	return FString("");
}

FGuid FSpout2MediaPlayer::GetPlayerPluginGUID() const
{
	static FGuid PlayerPluginGUID(0x6ca11ccb, 0x3f20bc6f, 0xd62160ea, 0xaaddb29b);
	return PlayerPluginGUID;
}

IMediaSamples& FSpout2MediaPlayer::GetSamples()
{
	return *this;
}

FString FSpout2MediaPlayer::GetStats() const
{
	return FString("");
}

IMediaTracks& FSpout2MediaPlayer::GetTracks()
{
	return *this;
}

FString FSpout2MediaPlayer::GetUrl() const
{
	return MediaUrl;
}

IMediaView& FSpout2MediaPlayer::GetView()
{
	return *this;
}

// Helper method to parse the frame rate from the sender name
void FSpout2MediaPlayer::ParseFrameRateFromSenderName(const FString& SenderName)
{
	// Check if the sender name contains FPS info
	if (SenderName.Contains(TEXT("|FPS=")))
	{
		FString FrameRateStr;
		if (SenderName.Split(TEXT("|FPS="), nullptr, &FrameRateStr))
		{
			FString NumeratorStr;
			FString DenominatorStr;
			
			if (FrameRateStr.Split(TEXT("/"), &NumeratorStr, &DenominatorStr))
			{
				int32 Numerator = FCString::Atoi(*NumeratorStr);
				int32 Denominator = FCString::Atoi(*DenominatorStr);
				
				if (Numerator > 0 && Denominator > 0)
				{
					FrameRate = FFrameRate(Numerator, Denominator);
				}
			}
			else
			{
				// If no denominator provided, assume it's a whole number
				int32 Fps = FCString::Atoi(*FrameRateStr);
				if (Fps > 0)
				{
					FrameRate = FFrameRate(Fps, 1);
				}
			}
		}
	}
}

bool FSpout2MediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	MediaUrl = Url;

	FString Scheme;
	FString Location;

	// check scheme
	if (!Url.Split(TEXT("://"), &Scheme, &Location, ESearchCase::CaseSensitive))
	{
		return false;
	}

	FString SourceName = Location;

	if (Scheme == FString("spout2mediain"))
	{
		CurrentState = EMediaState::Playing;
		SubscribeName = FName(SourceName);
		
		// Try to parse frame rate from the sender name
		ParseFrameRateFromSenderName(SourceName);
	}

	const USpout2MediaSource* Source = static_cast<const USpout2MediaSource*>(Options);
	if (Source)
	{
		bSRGB = Source->bSRGB;
		
		// Enable frame synchronization if requested in the source
		SetUseFrameSync(Source->bUseFrameSync);
		
		// Set the link rendering flag
		SetLinkRenderingToFrameSync(Source->bLinkRenderingToFrameSync);
		
		// Get frame rate from source if available and we didn't already get it from the sender name
		if (FrameRate == FFrameRate(60, 1))
		{
			FrameRate = Source->TargetFrameRate;
		}
		
		// Note: Since OnPreRender doesn't exist in this version of UE, we'll use a different approach
		// for frame synchronization (the WaitForSync method will handle this)
	}
	
	return true;
}

bool FSpout2MediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl,
	const IMediaOptions* Options)
{
	return Open(OriginalUrl, Options);
}

void FSpout2MediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
{
	unsigned int SpoutWidth = 0, SpoutHeight = 0;
	HANDLE SpoutShareHandle = nullptr;
	DXGI_FORMAT SpoutFormat = DXGI_FORMAT_UNKNOWN;

	const bool find_sender = senders.FindSender(
		TCHAR_TO_ANSI(*SubscribeName.ToString()), SpoutWidth, SpoutHeight, SpoutShareHandle, reinterpret_cast<DWORD&>(SpoutFormat));

	EPixelFormat PixelFormat = PF_Unknown;

	if (SpoutFormat == DXGI_FORMAT_B8G8R8A8_UNORM)
		PixelFormat = PF_B8G8R8A8;
	else if (SpoutFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
		PixelFormat = PF_FloatRGBA;
	else if (SpoutFormat == DXGI_FORMAT_R32G32B32A32_FLOAT)
		PixelFormat = PF_A32B32G32R32F;

	if (!find_sender
	|| PixelFormat == PF_Unknown)
		return;

	{
		if (!Context
			|| Context->Width != SpoutWidth
			|| Context->Height != SpoutHeight
			|| Context->DXFormat != SpoutFormat)
		{
			Context = MakeShared<FSpoutReceiverContext>(SpoutWidth, SpoutHeight, SpoutFormat);
		}
		
		ENQUEUE_RENDER_COMMAND(SpoutRecieverRenderThreadOp)([this, SpoutShareHandle](FRHICommandListImmediate& RHICmdList) {
			check(IsInRenderingThread());

			auto Sample = MakeShared<FSpout2MediaTextureSample, ESPMode::ThreadSafe>();
			FSpout2MediaTextureSample::InitializeArguments Args;
			Args.Width = Context->Width;
			Args.Height = Context->Height;
			Args.DXFormat = Context->DXFormat;
			Args.PixelFormat = Context->PixelFormat;
			Args.SpoutSharehandle = SpoutShareHandle;
			
			Args.Context = Context->Context;
			Args.D3D11Device = Context->D3D11Device;
			Args.D3D12Device = Context->D3D12Device;
			Args.D3D11on12Device = Context->D3D11on12Device;

			Args.bSRGB = this->bSRGB;
			
			Sample->Initialize(Args);
			
			if (TextureSample)
				TextureSample.Reset();
			
			TextureSample = Sample;
		});
	}
	
	// Update frame timestamp when new frame is received
	if (TextureSample.IsValid())
	{
		FrameTimeStamp = FPlatformTime::Cycles64();
	}
}

void FSpout2MediaPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
}

bool FSpout2MediaPlayer::GetPlayerFeatureFlag(EFeatureFlag flag) const
{
	return flag == IMediaPlayer::EFeatureFlag::AlwaysPullNewestVideoFrame
		|| flag == IMediaPlayer::EFeatureFlag::UseRealtimeWithVideoOnly;
}

bool FSpout2MediaPlayer::IsHardwareReady() const
{
	return CurrentState == EMediaState::Playing;
}

void FSpout2MediaPlayer::WaitForSync()
{
	if (CurrentState != EMediaState::Playing)
		return;
	
	// If frame sync is enabled, use sync events
	if (bUseFrameSync && FrameSyncHelper && !SubscribeName.IsNone())
	{
		// Extract source name without FPS info to match sender's actual name
		FString SourceName = GetSourceName();
		
		// Wait for frame sync event with a reasonable timeout
		if (FrameSyncHelper->WaitFrameSync(SourceName, 100))
		{
			// Update frame timestamp after successful sync
			FrameTimeStamp = FPlatformTime::Cycles64();
			return;
		}
	}
	
	// Fall back to the original frame timestamp comparison method
	int Count = 0;
	while (Count < 1000)
	{
		if (LastFrameTimeStamp != FrameTimeStamp)
		{
			LastFrameTimeStamp = FrameTimeStamp;
			break;
		}

		FPlatformProcess::SleepNoStats(0.001);
		Count += 1;
	}
}

bool FSpout2MediaPlayer::WaitForFrameSync(uint32 TimeoutMs)
{
	if (!bUseFrameSync || !FrameSyncHelper || SubscribeName.IsNone())
		return false;
	
	// Extract source name without FPS info to match sender's actual name
	FString SourceName = GetSourceName();
	
	// Wait for the sync event from the sender
	return FrameSyncHelper->WaitFrameSync(SourceName, TimeoutMs);
}

void FSpout2MediaPlayer::SetUseFrameSync(bool bEnable)
{
	bUseFrameSync = bEnable;
}

bool FSpout2MediaPlayer::IsFrameSyncEnabled() const
{
	return bUseFrameSync;
}

FString FSpout2MediaPlayer::GetSourceName() const
{
	if (SubscribeName.IsNone())
		return TEXT("");
	
	FString FullName = SubscribeName.ToString();
	FString BaseName;
	
	// Strip off any FPS information if present
	if (FullName.Split(TEXT("|FPS="), &BaseName, nullptr))
	{
		return BaseName;
	}
	
	return FullName;
}

/////

bool FSpout2MediaPlayer::FetchVideo(TRange<FTimespan> TimeRange,
	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample)
{
	if ((CurrentState != EMediaState::Paused) && (CurrentState != EMediaState::Playing))
	{
		return false; // nothing to play
	}

	if (!TextureSample)
		return false;
	
	// Update frame timestamp when delivering a frame
	FrameTimeStamp = FPlatformTime::Cycles64();
	
	// Get frame information from Spout
	if (Context)
	{
		// Use default frame rate if we can't determine it from Spout
		// (Spout doesn't provide direct frame rate info, so we use configured value)
	}
	
	OutSample = TextureSample;
	TextureSample.Reset();
	
	return true;
}

void FSpout2MediaPlayer::FlushSamples()
{
	IMediaSamples::FlushSamples();
	TextureSample.Reset();
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
IMediaSamples::EFetchBestSampleResult FSpout2MediaPlayer::FetchBestVideoSampleForTimeRange(
	const TRange<FMediaTimeStamp>& TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample,
	bool bReverse, bool bConsistentResult)
{
	return IMediaSamples::FetchBestVideoSampleForTimeRange(TimeRange, OutSample, bReverse, bConsistentResult);
}
#else
IMediaSamples::EFetchBestSampleResult FSpout2MediaPlayer::FetchBestVideoSampleForTimeRange(
	const TRange<FMediaTimeStamp>& TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample,
	bool bReverse)
{
	return IMediaSamples::FetchBestVideoSampleForTimeRange(TimeRange, OutSample, bReverse);
}
#endif

bool FSpout2MediaPlayer::PeekVideoSampleTime(FMediaTimeStamp& TimeStamp)
{
	return false;
}

int32 FSpout2MediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	if (TrackType == EMediaTrackType::Video)
		return 0;

	return INDEX_NONE;
}

FText FSpout2MediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (TrackType == EMediaTrackType::Video)
		return FText::FromString("Spout Video");
	
	return FText();
}

int32 FSpout2MediaPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (TrackType == EMediaTrackType::Video)
		return 0;

	return INDEX_NONE;
}

FString FSpout2MediaPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FString();
}

FString FSpout2MediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (TrackType == EMediaTrackType::Video)
		return FString("Spout Video");
	
	return FString();
}

bool FSpout2MediaPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex,
	FMediaVideoTrackFormat& OutFormat) const
{
	OutFormat.FrameRate = 0;
	OutFormat.FrameRates = TRange<float>(0, 0);

	if (TextureSample)
	{
		OutFormat.Dim = TextureSample->GetDim();
	}
	
	return false;
}

bool FSpout2MediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	return false;
}

bool FSpout2MediaPlayer::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return false;
}

bool FSpout2MediaPlayer::SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float InFrameRate)
{
	return false;
}
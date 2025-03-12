// Fill out your copyright notice in the Description page of Project Settings.

#include "Spout2MediaCustomTimeStep.h"
#include "MediaPlayerFacade.h"

USpout2MediaCustomTimeStep::USpout2MediaCustomTimeStep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USpout2MediaCustomTimeStep::Initialize(UEngine* InEngine)
{
	if (!MediaPlayer)
		return false;
	
	return true;
}

void USpout2MediaCustomTimeStep::Shutdown(UEngine* InEngine)
{
	SpoutMediaPlayer.Reset();
}

bool USpout2MediaCustomTimeStep::UpdateTimeStep(UEngine* InEngine)
{
	auto PlayerAsset = MediaPlayer.LoadSynchronous();
	if (PlayerAsset == nullptr)
		return false;
	
	auto Player = StaticCastSharedPtr<FSpout2MediaPlayer>(PlayerAsset->GetPlayerFacade()->GetPlayer());
	if (Player == nullptr || !Player.IsValid())
		return false;

	SpoutMediaPlayer = Player;

	const bool bWaitedForSync = WaitForSync();

	return true;
}

ECustomTimeStepSynchronizationState USpout2MediaCustomTimeStep::GetSynchronizationState() const
{
	if (!SpoutMediaPlayer || !SpoutMediaPlayer->IsHardwareReady())
		return ECustomTimeStepSynchronizationState::Closed;

	return ECustomTimeStepSynchronizationState::Synchronized;
}

FFrameRate USpout2MediaCustomTimeStep::GetFixedFrameRate() const
{
	if (!SpoutMediaPlayer || !SpoutMediaPlayer->IsHardwareReady())
		return FFrameRate(60, 1);

	return SpoutMediaPlayer->GetFrameRate();
}

uint32 USpout2MediaCustomTimeStep::GetLastSyncCountDelta() const
{
	return 1;
}

bool USpout2MediaCustomTimeStep::IsLastSyncDataValid() const
{
	if (!SpoutMediaPlayer || !SpoutMediaPlayer->IsHardwareReady())
		return false;

	return true;
}

bool USpout2MediaCustomTimeStep::WaitForSync()
{
	if (!SpoutMediaPlayer || !SpoutMediaPlayer->IsHardwareReady())
		return false;

	// Use the frame sync waiting mechanism
	if (SpoutMediaPlayer->IsFrameSyncEnabled())
	{
		return SpoutMediaPlayer->WaitForFrameSync();
	}
	else
	{
		// Fall back to the standard waiting method
		SpoutMediaPlayer->WaitForSync();
		return true;
	}
}
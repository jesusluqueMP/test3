// Frame synchronization helper for Spout2Media

#pragma once

#include "CoreMinimal.h"
#include "Windows/AllowWindowsPlatformTypes.h" 
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

/**
 * Helper class to manage Spout frame synchronization between senders and receivers
 */
class FSpoutFrameSyncHelper
{
public:
    FSpoutFrameSyncHelper();
    ~FSpoutFrameSyncHelper();

    // Frame rate control
    void HoldFps(int fps);
    
    // Frame counting management
    void DisableFrameCount();
    bool IsFrameCountEnabled() const;
    
    // Sync events
    void SetFrameSync(const FString& SenderName);
    bool WaitFrameSync(const FString& SenderName, DWORD dwTimeout);
    
    // Clear any existing sync events
    void ClearFrameSync(const FString& SenderName);

private:
    // FPS control
    bool bFrameCountEnabled;
    int TargetFPS;
    double LastFrameTime;
    double FrameInterval;
    
    // Time tracking
    uint64 FrameCount;
    
    // Event handling
    TMap<FString, HANDLE> SyncEvents;
    FCriticalSection SyncEventsLock;
    
    // Helper methods
    FString GetEventName(const FString& SenderName);
};
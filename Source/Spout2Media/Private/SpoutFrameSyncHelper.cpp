#include "SpoutFrameSyncHelper.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformTime.h"

FSpoutFrameSyncHelper::FSpoutFrameSyncHelper()
    : bFrameCountEnabled(true)
    , TargetFPS(60)
    , LastFrameTime(0.0)
    , FrameInterval(1.0/60.0)
    , FrameCount(0)
{
}

FSpoutFrameSyncHelper::~FSpoutFrameSyncHelper()
{
    FScopeLock Lock(&SyncEventsLock);
    // Clean up all sync events
    for (TPair<FString, HANDLE>& Event : SyncEvents)
    {
        if (Event.Value != NULL && Event.Value != INVALID_HANDLE_VALUE)
        {
            CloseHandle(Event.Value);
            Event.Value = NULL;
        }
    }
    SyncEvents.Empty();
}

void FSpoutFrameSyncHelper::HoldFps(int fps)
{
    if (!bFrameCountEnabled || fps <= 0)
        return;
    
    TargetFPS = fps;
    FrameInterval = 1.0 / static_cast<double>(fps);
    
    // Calculate wait time to achieve target FPS
    double CurrentTime = FPlatformTime::Seconds();
    double ElapsedTime = CurrentTime - LastFrameTime;
    
    // Only wait if we're ahead of schedule
    if (ElapsedTime < FrameInterval)
    {
        double WaitTime = FrameInterval - ElapsedTime;
        
        // Convert to milliseconds for FPlatformProcess::Sleep
        const float SleepTime = static_cast<float>(WaitTime * 1000.0);
        
        if (SleepTime > 1.0f)  // Only sleep for meaningful durations
        {
            FPlatformProcess::Sleep(SleepTime / 1000.0f); // Sleep takes seconds
        }
    }
    
    // Update last frame time after any waiting
    LastFrameTime = FPlatformTime::Seconds();
    FrameCount++;
}

void FSpoutFrameSyncHelper::DisableFrameCount()
{
    bFrameCountEnabled = false;
}

bool FSpoutFrameSyncHelper::IsFrameCountEnabled() const
{
    return bFrameCountEnabled;
}

FString FSpoutFrameSyncHelper::GetEventName(const FString& SenderName)
{
    // Format event name to ensure uniqueness
    return FString::Printf(TEXT("Spout-Sync-%s"), *SenderName);
}

void FSpoutFrameSyncHelper::SetFrameSync(const FString& SenderName)
{
    if (SenderName.IsEmpty())
        return;
    
    FScopeLock Lock(&SyncEventsLock);
    
    const FString EventName = GetEventName(SenderName);
    HANDLE SyncEvent = NULL;
    
    // Check if we already have this event
    if (SyncEvents.Contains(SenderName))
    {
        SyncEvent = SyncEvents[SenderName];
    }
    else
    {
        // Create a new sync event
        SyncEvent = CreateEventA(NULL, false, false, TCHAR_TO_ANSI(*EventName));
        if (SyncEvent != NULL && SyncEvent != INVALID_HANDLE_VALUE)
        {
            SyncEvents.Add(SenderName, SyncEvent);
        }
    }
    
    // Signal the event
    if (SyncEvent != NULL && SyncEvent != INVALID_HANDLE_VALUE)
    {
        SetEvent(SyncEvent);
    }
}

bool FSpoutFrameSyncHelper::WaitFrameSync(const FString& SenderName, DWORD dwTimeout)
{
    if (SenderName.IsEmpty())
        return false;
    
    HANDLE SyncEvent = NULL;
    
    {
        FScopeLock Lock(&SyncEventsLock);
        const FString EventName = GetEventName(SenderName);
        
        // Check if we have this event
        if (SyncEvents.Contains(SenderName))
        {
            SyncEvent = SyncEvents[SenderName];
        }
        else
        {
            // Try to open existing event
            SyncEvent = OpenEventA(SYNCHRONIZE, false, TCHAR_TO_ANSI(*EventName));
            if (SyncEvent != NULL && SyncEvent != INVALID_HANDLE_VALUE)
            {
                SyncEvents.Add(SenderName, SyncEvent);
            }
            else
            {
                // If event doesn't exist yet, create it
                SyncEvent = CreateEventA(NULL, false, false, TCHAR_TO_ANSI(*EventName));
                if (SyncEvent != NULL && SyncEvent != INVALID_HANDLE_VALUE)
                {
                    SyncEvents.Add(SenderName, SyncEvent);
                }
                else
                {
                    return false;
                }
            }
        }
    }
    
    // Wait for the event
    DWORD WaitResult = WaitForSingleObject(SyncEvent, dwTimeout);
    return (WaitResult == WAIT_OBJECT_0);
}

void FSpoutFrameSyncHelper::ClearFrameSync(const FString& SenderName)
{
    if (SenderName.IsEmpty())
        return;
    
    FScopeLock Lock(&SyncEventsLock);
    
    if (SyncEvents.Contains(SenderName))
    {
        HANDLE SyncEvent = SyncEvents[SenderName];
        if (SyncEvent != NULL && SyncEvent != INVALID_HANDLE_VALUE)
        {
            CloseHandle(SyncEvent);
        }
        SyncEvents.Remove(SenderName);
    }
}
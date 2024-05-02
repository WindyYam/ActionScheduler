#include <stdio.h>
#include "app_framework.h"
#include "action_scheduler.h"
#include "critical_section.h"

#define MIN_WAKEUP_SAFEZONE_MS 0U
#define MIN_SUSPEND_TIME_DELAY 1U

// This app_framework is mainly for a wrapper environment to run action scheduler as app/task/event framework, and manage low power mode sleep time based on event scheduling
static uint32_t lastHalTick = 0;
static uint32_t lastRtcTick = 0;
static bool suspendEnabled = true;
#ifdef USE_WAKE_LOCK
static uint8_t powerLockRecursive = 0;  // Enable low power when lock is 0
#endif
static bool suspendedLastRound = false;
// This module assumes a 32768hz RTC
static RTC_HandleTypeDef* rtc = NULL;

// used to abort the suspend if the timeline has updated due to interrupt and nearest event delay is now less than suspendTime
static bool ShouldAbortSuspend(uint32_t suspendTime)
{
    return ActionScheduler_GetNextEventDelay() < suspendTime;
}

__attribute__((weak)) void AppFramework_PreSuspendHook()
{
    // Override in user code for things you want to do before suspend
    // e.g. flush Uart TX
}

__attribute__((weak)) void AppFramework_PostSuspendHook()
{
    // Override in user code for things you want to do after wakeup
    // e.g. restart PLL if in stop1 mode
}

static void Suspend(uint32_t timeInMs, bool* updateFlag)
{
  timeInMs = timeInMs > MIN_WAKEUP_SAFEZONE_MS ? timeInMs - MIN_WAKEUP_SAFEZONE_MS : 0U;   // A safezone to wake up a little bit earlier
  const uint32_t clk = 2048;  // 2048HZ RTC wakeup clock (32768 / DIV16)
  // The RTC alarm can have only UINT16_MAX count number
  uint32_t cnt = (timeInMs >= (UINT16_MAX * 1000U / clk))? UINT16_MAX : timeInMs * clk / 1000U;
  if(cnt >= MIN_SUSPEND_TIME_DELAY)
  {
    uint32_t msTime = cnt * 1000U / clk;
    DEBUG_PRINTF("Sleep for %dms...\n", msTime);
    AppFramework_PreSuspendHook();
    Enter_Critical();
    // Interrupt scheduling in rare case can happen above the Enter_Critical line, result in most recent action delay updated
    if(ShouldAbortSuspend(msTime))
    {
      Exit_Critical();
      DEBUG_PRINTF("Abort sleep\n");
      *updateFlag = false;
      return;
    }
    *updateFlag = true;
    HAL_SuspendTick();
    (void)HAL_RTCEx_SetWakeUpTimer_IT(rtc, cnt - 1U, RTC_WAKEUPCLOCK_RTCCLK_DIV16);
#ifdef USE_STOP1_MODE
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
#else
    HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    HAL_PWREx_DisableLowPowerRunMode();
#endif
    Exit_Critical();
    HAL_ResumeTick();
    AppFramework_PostSuspendHook();

  }
  else
  {
    *updateFlag = false;
  }
}

// This call will return the duration to the beginning of timeline, used on AppFramework_Schedule for an absolute time scheduling
static inline uint32_t GetDurationToTimelineBeginning()
{
    if (suspendedLastRound)
    {
        return AppFramework_GetRTCDuration(AppFramework_GetRTCTimestamp(), lastRtcTick) - ActionScheduler_GetProceedingTime();
    }
    else
    {
        return (HAL_GetTick() - lastHalTick) - ActionScheduler_GetProceedingTime();
    }
}

uint32_t AppFramework_GetRTCDuration(uint32_t currentTimeStamp, uint32_t lastTimeStamp)
{
    if (currentTimeStamp < lastTimeStamp)
    {
        // overflow, add the missing 24 hours back
        currentTimeStamp += 24U * 3600000U;
    }
    return currentTimeStamp - lastTimeStamp;
}

uint32_t AppFramework_GetRTCTimestamp()
{
    RTC_TimeTypeDef rtcTime;
    RTC_DateTypeDef rtcDate;
    (void)HAL_RTC_GetTime(rtc, &rtcTime, RTC_FORMAT_BIN);
    (void)HAL_RTC_GetDate(rtc, &rtcDate, RTC_FORMAT_BCD);
    return (rtcTime.Hours * 3600000U) + (rtcTime.Minutes * (uint32_t)60000U) + (rtcTime.Seconds * (uint32_t)1000U) + ((1000U * (rtcTime.SecondFraction - rtcTime.SubSeconds)) / (rtcTime.SecondFraction + 1U));
}

void AppFramework_WakeLockRecursive(bool hold)
{
#ifdef USE_WAKE_LOCK
    Enter_Critical();
    if(hold)
    {
        powerLockRecursive += 1U;
    }
    else
    {
        powerLockRecursive -= 1U;
    }
    Exit_Critical();
    if(hold)
    {
        DEBUG_PRINTF("Hold WakeLock %d\n", powerLockRecursive);
    }
    else
    {
        DEBUG_PRINTF("Release WakeLock %d\n", powerLockRecursive);
    }
#endif
}

// This schedule delay is relative to the timestamp when it executes, compare to to ActionScheduler_Schedule which is based on the head of the timeline at that moment
// The ActionScheduler_Proceed calls might not be continuous process when sleep is involved. In this case if you use ActionScheduler_Schedule for a delayed action (e.g. from wakeup ISR) before the timeline proceed, it will be executed in less than expected delay
// For example, if you've slept 10 secs, and get wake up by an interrupt which runs ISR to ActionScheduler_Schedule a 5 secs action, this action will be executed right away, as timeline head is going to proceed 10 sec, and your 5s action is relative to the beginning of timeline.
// Compare to ActionScheduler_Schedule for which delay is relative to the last round ActionScheduler_Proceed get called, this function make sure delay is based on the absolute time when it is called
// So, this function can be used on wakeup ISR, as it is delay to the current absolute time when it executes, the delay will be precise
// On the other hand, if you do want the delay to be based on the current timeline beginning(where the last proceed ends or in the middle of a proceed), use ActionScheduler_Schedule directly
// Compare these 2 calls: imagine 5s elapsed to last round, timeline is going to proceed 5s, and you have an event in 1s which also schedule another 1s delay event in the callback,
// 1. If the new event is scheduled by ActionScheduler_Schedule directly, the 5s proceed will fire both events, as the new event delay is based on where the first event is on
// 2. If the new event is scheduled by AppFramework_Schedule, the 5s proceed will fire the first event only, and another 1s passed the new event will fire, as AppFramework_Schedule is based on the current absolute time it executes
ActionSchedulerId_t AppFramework_Schedule(uint32_t delayedTimeInMs, ActionCallback_t cb, void *arg)
{
    return ActionScheduler_ScheduleReload(GetDurationToTimelineBeginning() + delayedTimeInMs, delayedTimeInMs, cb, arg);
}
// If you want the reload to be different to the delay
ActionSchedulerId_t AppFramework_ScheduleReload(uint32_t delayedTimeInMs, uint32_t reloadTimeInMs, ActionCallback_t cb, void *arg)
{
    return ActionScheduler_ScheduleReload(GetDurationToTimelineBeginning() + delayedTimeInMs, reloadTimeInMs, cb, arg);
}

bool AppFramework_Unschedule(ActionSchedulerId_t *actionId)
{
    return ActionScheduler_Unschedule(actionId);
}

bool AppFramework_UnscheduleAll(ActionCallback_t cb)
{
    return ActionScheduler_UnscheduleAll(cb);
}

void AppFramework_SetSuspendEnable(bool en)
{
    suspendEnabled = en;
    DEBUG_PRINTF("Suspend enable: %d\n", en);
}

__attribute__((weak)) void DEBUG_PRINTF(const char* fmt, ...)
{
    // Default implementation using printf
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(stderr, fmt, argptr);
    va_end(argptr);
}

__attribute__((weak)) void DEBUG_PRINTF_NOBUFFER(const char* fmt, ...)
{
    // Default implementation using printf
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(stderr, fmt, argptr);
    va_end(argptr);
}

void AppFramework_Init(RTC_HandleTypeDef* hrtc)
{
    rtc = hrtc;
    ActionScheduler_Clear();
    lastRtcTick = AppFramework_GetRTCTimestamp();
    lastHalTick = HAL_GetTick();
    DEBUG_PRINTF("********************\n");
    DEBUG_PRINTF("App Framework Start\n");
    DEBUG_PRINTF("********************\n");
}

void AppFramework_Loop()
{
    // In this framework, system sleep for most of the time until
    // interrupt wake it up to process ISR code, or
    // the next scheduled action is about to occur, and wake up by RTC.
    // After that, system go back to sleep again.
    // So, the ActionScheduler module takes a very fundamental part in this framework.
    // For all user works, you should use AppFramework_Schedule to post action event all the time
    // For ISR, post the work load to AppFramework_Schedule to run in normal context
    // Waked up, either by RTC or by interrupt
    uint32_t rtctick = AppFramework_GetRTCTimestamp();
    uint32_t haltick = HAL_GetTick();
    // if suspendedLastRound, the sys tick is not reliable, we need RTC tick for calculation of duration
    // otherwise, use systick as it has more precision (looks like rtc timestamp is in resolution of 4ms, while systick is 1ms)
    if (suspendedLastRound)
    {
        uint32_t elapsed = AppFramework_GetRTCDuration(rtctick, lastRtcTick);
        DEBUG_PRINTF("Wake up from %dms\n", elapsed);
        (void)ActionScheduler_Proceed(elapsed);
        suspendedLastRound = false;
    }
    else
    {
        (void)ActionScheduler_Proceed(haltick - lastHalTick);
    }

    // Synchronize the timestamp and proceeding time, lock needed here as no ISR schedule is allowed in between
    Enter_Critical();
    lastRtcTick = rtctick;
    lastHalTick = haltick;
    ActionScheduler_ClearProceedingTime();
    Exit_Critical();
    if(suspendEnabled)
    {
#ifdef USE_WAKE_LOCK
        if (powerLockRecursive == 0U)
#endif
        {
            uint32_t nextEventDelay = ActionScheduler_GetNextEventDelay();
            Suspend(nextEventDelay, &suspendedLastRound);
        }
    }
}
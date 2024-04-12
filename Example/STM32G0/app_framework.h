#ifndef __APP_FRAMEWORK_H
#define __APP_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "action_scheduler.h"
#include <stm32g0xx_hal.h>

#ifdef DEBUG_PRINT
void DEBUG_PRINTF(const char* fmt, ...);
// No buffer version is for print in ISR, which does block printing rather than put into print buffer
void DEBUG_PRINTF_NOBUFFER(const char* fmt, ...);
#else
#define DEBUG_PRINTF(fmt, ...)
#define DEBUG_PRINTF_NOBUFFER(fmt, ...)
#endif

#ifdef USE_STOP1_MODE
#define USE_WAKE_LOCK
#endif

void AppFramework_Init();
void AppFramework_Loop();
void AppFramework_WakeLockRecursive(bool hold);
ActionSchedulerId_t AppFramework_Schedule(uint32_t delayedTimeInMs, ActionCallback_t cb, void* arg);
ActionSchedulerId_t AppFramework_ScheduleWithReload(uint32_t delayedTimeInMs, uint32_t reloadInMs, ActionCallback_t cb, void *arg);
bool AppFramework_UnscheduleById(ActionSchedulerId_t* actionId);
bool AppFramework_UnscheduleByCallback(ActionCallback_t cb);
uint32_t AppFramework_GetRTCDuration(uint32_t currentTimeStamp, uint32_t lastTimeStamp);
uint32_t AppFramework_GetRTCTimestamp();
void AppFramework_SetSuspendEnable(bool en);

void AppFramework_PreSuspendHook();
void AppFramework_PostSuspendHook();

#ifdef __cplusplus
}
#endif

#endif /* __APP_FRAMEWORK_H */

#ifndef __ACTION_SCHEDULER_H
#define __ACTION_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef MAX_ACTION_SCHEDULER_NODES
#define MAX_ACTION_SCHEDULER_NODES 64
#endif

#if MAX_ACTION_SCHEDULER_NODES > 255
#error MAX_ACTION_SCHEDULER_NODES can not exceed 255! For now
#endif

#define ACTION_SCHEDULER_ID_INVALID 0

typedef enum{
    ACTION_ONESHOT,
    ACTION_RELOAD
}ActionReturn_t;
// The return value indicate if you want to schedule again after finish, in same interval
typedef ActionReturn_t (*ActionCallback_t)(void* arg);
typedef uint16_t ActionSchedulerId_t;

bool ActionScheduler_Proceed(uint32_t timeElapsedMsSinceLastCall);
ActionSchedulerId_t ActionScheduler_Schedule(uint32_t delayedTimeInMs, ActionCallback_t cb, void* arg);
ActionSchedulerId_t ActionScheduler_ScheduleWithReload(uint32_t delayedTime, uint32_t reload, ActionCallback_t cb, void* arg);
bool ActionScheduler_UnscheduleById(ActionSchedulerId_t* actionId);
bool ActionScheduler_UnscheduleByCallback(ActionCallback_t cb);
bool ActionScheduler_Clear(void);
uint32_t ActionScheduler_GetNextEventDelay(void);
uint32_t ActionScheduler_GetProceedingTime(void);
void ActionScheduler_ClearProceedingTime(void);
bool ActionScheduler_IsCallbackArmed(ActionCallback_t cb);

#ifdef __cplusplus
}
#endif
#endif /* __ACTION_SCHEDULER_H */
#ifndef ACTION_SCHEDULER_H
#define ACTION_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef MAX_ACTION_SCHEDULER_NODES
#define MAX_ACTION_SCHEDULER_NODES 64U
#endif

#if MAX_ACTION_SCHEDULER_NODES >= 255
#error MAX_ACTION_SCHEDULER_NODES can not exceed 255! For now
#endif

// If you are to refactor the MAX_ACTION_SCHEDULER_NODES to be greater than 254, pay attention to this constant, make it right
#define ACTION_SCHEDULER_ID_INVALID UINT16_MAX

typedef enum{
    ACTION_ONESHOT,
    ACTION_RELOAD
}ActionReturn_t;
// The return value indicate if you want to schedule again after finish, in same interval
typedef ActionReturn_t (*ActionCallback_t)(void* arg);
typedef uint16_t ActionSchedulerId_t;

bool ActionScheduler_Proceed(uint32_t timeElapsedMs);
ActionSchedulerId_t ActionScheduler_Schedule(uint32_t delayedTime, ActionCallback_t cb, void* arg);
ActionSchedulerId_t ActionScheduler_ScheduleReload(uint32_t delayedTime, uint32_t reload, ActionCallback_t cb, void* arg);
bool ActionScheduler_Unschedule(ActionSchedulerId_t* actionId);
bool ActionScheduler_UnscheduleAll(ActionCallback_t cb);
void ActionScheduler_Clear(void);
uint32_t ActionScheduler_GetNextEventDelay(void);
uint32_t ActionScheduler_GetProceedingTime(void);
void ActionScheduler_ClearProceedingTime(void);
bool ActionScheduler_IsCallbackArmed(ActionCallback_t cb);
uint16_t ActionScheduler_GetActiveNodesWaterMark(void);

#ifdef __cplusplus
}
#endif
#endif /* ACTION_SCHEDULER_H */
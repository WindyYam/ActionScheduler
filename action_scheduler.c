//
// Author: zhenya.y@lx-group.com.au
//
// The main purpose of this module is to provide a convenient way of scheduling a delayed function call or periodic function call
// This module use a timeline based linked list to manage the schedule function calls. The linked list is statically allocated in an array without heap involved
// The earlier node will always be closer to the (logical) head, later node to the (logical) tail
// New events will be inserted into timeline based on the relative time
// Thus, the efficiency is guaranteed as we don't need to travse the whole linked list every time we proceed the time
// Periodic events is inserted the same as one shot event except it will reload a new event after previous one expires
// Event callback chain is also supported, means you can schedule another one or multiple new events at the end of one event callback
// This module can be used on ISR as well to push a function call to run out of ISR context
// This module is also useful to cooperate with low power usage as everything is managed in timeline, thus also the sleep time is determined
// A schedule is simply an invocation ActionScheduler_Schedule, no any configuration needed, no static data needed, just the function, the arg and the delay
// The arg is a void pointer which normally is 4 bytes, that you can cast into anything like integer, float, bool, or generically a pointer to your data
// For unschedule, the safety is enforced by local counter and magic number, so unscheduling an expired event won't cause too much trouble at the moment
//
#include "action_scheduler.h"
#include "critical_section.h"
#include <stddef.h>

// For __packed struct
#if defined ( __CC_ARM ) || defined (__ARMCC_VERSION) || (defined (__arm__) && defined ( __GNUC__ )) || defined ( __ICCARM__ ) || defined ( __TI_ARM__ )
#include <cmsis_compiler.h>
#else
#define __PACKED_STRUCT struct
#endif
typedef __PACKED_STRUCT
{
    ActionCallback_t callback;
    uint32_t delayToPrevious;
    uint32_t reload;
    void* arg;
    uint8_t usedCounter;
    uint8_t previousNodeIdx;
    uint8_t nextNodeIdx;
}ActionNode_t;

static ActionNode_t mNodes[MAX_ACTION_SCHEDULER_NODES] = {0};
static uint8_t mNodeStartIdx = 0;
static uint8_t mNodeEndIdx = 0;
static uint16_t mActiveNodes = 0;
static uint32_t mProceedingTime = 0;    // specific for scheduling inside callback case

// Lock/Unlock for interrupt/thread safety, if we want to use the functions in interrupt handler/multi threads
// But if not, we don't need these
static inline void ListLock(void)
{
    Enter_Critical();
}
static inline void ListUnlock(void)
{
    Exit_Critical();
}

static inline bool getFreeSlot(uint8_t* slotIdx)
{
    bool ret = false;
    //find next available slot starting from the end
    for (uint8_t i = (mNodeEndIdx + 1U) % MAX_ACTION_SCHEDULER_NODES; i != mNodeEndIdx; i = (i + 1U) % MAX_ACTION_SCHEDULER_NODES)
    {
        if (mNodes[i].callback == NULL)
        {
            *slotIdx = i;
            ret = true;
            break;
        }
    }
    return ret;
}

static inline uint16_t generateActionIdAt(uint8_t idx)
{
    return idx | ((uint16_t)mNodes[idx].usedCounter << 8);
}

static inline void removeNodeAt(uint8_t idx)
{
    if(idx < MAX_ACTION_SCHEDULER_NODES)
    {
        mNodes[idx].callback = NULL;
        if (mActiveNodes > 1U)
        {
            if (idx == mNodeStartIdx)
            {
                uint8_t nextCursor = mNodes[idx].nextNodeIdx;
                mNodes[nextCursor].previousNodeIdx = nextCursor;
                mActiveNodes -= 1U;
                uint32_t timeleft = mNodes[mNodeStartIdx].delayToPrevious;
                mNodeStartIdx = nextCursor;
                mNodes[mNodeStartIdx].delayToPrevious += timeleft;
            }
            else if (idx == mNodeEndIdx)
            {
                uint8_t previousCursor = mNodes[idx].previousNodeIdx;
                mNodes[previousCursor].nextNodeIdx = previousCursor;
                mNodeEndIdx = previousCursor;
                mActiveNodes -= 1U;
            }
            else
            {
                if((mNodes[idx].previousNodeIdx == idx) && (mNodes[idx].nextNodeIdx == idx))
                {
                    // this is not the start node nor the end node, but its next and previous are itself, means this is an isolated node not in the timeline
                    // could be the product of a reschedule in the middle i.e. from a ActionScheduler callback, nothing to do for the timeline
                    return;
                }
                uint8_t previousCursor = mNodes[idx].previousNodeIdx;
                uint8_t nextCursor = mNodes[idx].nextNodeIdx;
                mNodes[previousCursor].nextNodeIdx = nextCursor;
                mNodes[nextCursor].previousNodeIdx = previousCursor;
                mNodes[nextCursor].delayToPrevious += mNodes[idx].delayToPrevious;
                mActiveNodes -= 1U;
            }
        }
        else if (mActiveNodes == 1U)
        {
            if(idx != mNodeStartIdx)
            {
                // This is an isolated node
                return;
            }
            //only the head node
            mActiveNodes = 0;
            mNodeStartIdx = idx;
            mNodeEndIdx = idx;
        }
        else
        {
            // No node, do nothing
        }
    }
}

static inline void insertNode(uint8_t idx, uint32_t delay)
{
    int16_t idxA = -1, idxB = (int16_t)mNodeStartIdx;
    //find the correct location for the new node in the linked list, starting from first node
    while (mNodes[idxB].delayToPrevious <= delay)
    {
        delay = delay - mNodes[idxB].delayToPrevious;
        idxA = idxB;
        if (idxB == (int16_t)mNodeEndIdx) //end
        {
            idxB = -1;
            break;
        }
        else
        {
            idxB = (int16_t)mNodes[idxB].nextNodeIdx;
        }
    }
    mNodes[idx].delayToPrevious = delay;
    // Insert node
    if (idxA < 0)
    {
        //this means node should be inserted only before idxB, and in this situation idxB is the old start
        mNodes[idx].previousNodeIdx = idx;
        mNodes[idx].nextNodeIdx = (uint8_t)idxB;
        mNodes[idxB].previousNodeIdx = idx;
        mNodes[idxB].delayToPrevious = mNodes[idxB].delayToPrevious - mNodes[idx].delayToPrevious;
        mNodeStartIdx = idx;
    }
    else if (idxB < 0)
    {
        //this means node should be inserted only after idxA, and in this situation idxA is the old end
        mNodes[idx].previousNodeIdx = (uint8_t)idxA;
        mNodes[idx].nextNodeIdx = idx; //set it to self as the end
        mNodes[idxA].nextNodeIdx = idx;
        mNodeEndIdx = idx;
    }
    else
    {
        //normal insertion between 2 nodes
        mNodes[idx].previousNodeIdx = (uint8_t)idxA;
        mNodes[idx].nextNodeIdx = (uint8_t)idxB;
        mNodes[idxA].nextNodeIdx = idx;
        mNodes[idxB].previousNodeIdx = idx;
        mNodes[idxB].delayToPrevious -= mNodes[idx].delayToPrevious;
    }
}

bool ActionScheduler_Proceed(uint32_t timeElapsedMs)
{
    bool ret = false;
    ListLock();

    while ((timeElapsedMs >= mNodes[mNodeStartIdx].delayToPrevious) && (mActiveNodes > 0U))
    {
        timeElapsedMs -= mNodes[mNodeStartIdx].delayToPrevious;
        mProceedingTime += mNodes[mNodeStartIdx].delayToPrevious;
        uint8_t currentCursor = mNodeStartIdx;
        mActiveNodes -= 1U;
        ActionCallback_t cb = mNodes[currentCursor].callback;
        void* arg = mNodes[currentCursor].arg;
        if (mActiveNodes > 0U)
        {
            // isolate the node out from the timeline
            uint8_t nextCursor = mNodes[currentCursor].nextNodeIdx;
            mNodes[nextCursor].previousNodeIdx = nextCursor;
            mNodeStartIdx = nextCursor;
            mNodes[currentCursor].nextNodeIdx = currentCursor;
        }
        else
        {
            mNodes[currentCursor].nextNodeIdx = currentCursor;
        }
        // This whole function should be inside the lock, but here we need to unlock as for the callback chain
        ListUnlock();
        ActionReturn_t actionRet = cb(arg);
        ListLock();
        switch(actionRet)
        {
            case ACTION_ONESHOT:
                mNodes[currentCursor].callback = NULL;
            break;
            case ACTION_RELOAD:
                // The callback can unschedule this, result in callback changed to null, we need to check this
                if(mNodes[currentCursor].callback != NULL)
                {
                    if (mActiveNodes == 0U) //the linked list is empty, this is the first node
                    {
                        mNodes[currentCursor].delayToPrevious = mNodes[currentCursor].reload;
                    }
                    else
                    {
                        insertNode(currentCursor, mNodes[currentCursor].reload);
                    }
                    mActiveNodes++;
                }
            break;
            default:
                // Nothing
            break;
        }
        ret = true;
    }

    if (mActiveNodes > 0U)
    {
        mNodes[mNodeStartIdx].delayToPrevious -= timeElapsedMs;
        mProceedingTime += timeElapsedMs;
    }
    
    ListUnlock();
    return ret;
}

// The delay is relative to the current head of the linked list, or timeline
// delayedTime is the initial delay you want to fire the callback, reload is the reload value for next call when callback returns ACTION_RELOAD. Sometimes you would like them to be different
ActionSchedulerId_t ActionScheduler_ScheduleReload(uint32_t delayedTime, uint32_t reload, ActionCallback_t cb, void* arg)
{
    uint16_t ActionSchedulerId = ACTION_SCHEDULER_ID_INVALID;
    if ((cb == NULL) || (mActiveNodes >= MAX_ACTION_SCHEDULER_NODES))
    {
        return ActionSchedulerId;
    }
    
    // The algorithm basically insert the new Node into existing timeline of linked list
    ListLock();
    if (mActiveNodes == 0U) //the linked list is empty, this is the first node
    {
        // In case of a reserved node which is used in periodic scheduling, we need to look for a one with empty callback
        uint8_t freeCursor = mNodeStartIdx;
        if(!getFreeSlot(&freeCursor))
        {
            ListUnlock();
            return ACTION_SCHEDULER_ID_INVALID;
        }
        mNodes[freeCursor].usedCounter++;
        mNodes[freeCursor].previousNodeIdx = freeCursor;
        mNodes[freeCursor].nextNodeIdx = freeCursor; //set it to self as the end
        mNodes[freeCursor].delayToPrevious = delayedTime;
        mNodes[freeCursor].callback = cb;
        mNodes[freeCursor].arg = arg;
        mNodes[freeCursor].reload = reload;
        mNodeStartIdx = freeCursor;
        mNodeEndIdx = freeCursor;
        mActiveNodes += 1U;
        ActionSchedulerId = generateActionIdAt(freeCursor);
    }
    else
    {
        uint8_t freeCursor = mNodeEndIdx;
        if(!getFreeSlot(&freeCursor))
        {
            ListUnlock();
            return ACTION_SCHEDULER_ID_INVALID;
        }

        mNodes[freeCursor].usedCounter++;
        mNodes[freeCursor].callback = cb;
        mNodes[freeCursor].arg = arg;
        mNodes[freeCursor].delayToPrevious = delayedTime;
        mNodes[freeCursor].reload = reload;
        mActiveNodes += 1U;

        insertNode(freeCursor, delayedTime);
        ActionSchedulerId = generateActionIdAt(freeCursor);
    }
    ListUnlock();
    return ActionSchedulerId;
}

ActionSchedulerId_t ActionScheduler_Schedule(uint32_t delayedTime, ActionCallback_t cb, void* arg)
{
    return ActionScheduler_ScheduleReload(delayedTime, delayedTime, cb, arg);
}

// The safety for the unscheduling is enforced by a local counter in each node, but the counter still round back after exactly 256 schedule calls
// Keep that in mind, bad luck exists, but generally it is safe to do an unschedule to a finished ActionScheduler
bool ActionScheduler_Unschedule(ActionSchedulerId_t* actionId)
{
    bool ret = false;
    if (*actionId != ACTION_SCHEDULER_ID_INVALID)
    {
        ListLock();
        uint8_t id = (uint8_t)(*actionId & 0xffU);
        uint8_t counter = (uint8_t)(*actionId >> 8U);
        if ((id < MAX_ACTION_SCHEDULER_NODES) && (mNodes[id].callback != NULL) && (mNodes[id].usedCounter == counter))
        {
            ret = true;
            removeNodeAt(id);
            *actionId = ACTION_SCHEDULER_ID_INVALID;
        }
        ListUnlock();
    }
    return ret;
}

// Relatively safer to the version that use ActionSchedulerId, and it traverse through all the internal linked list node
bool ActionScheduler_UnscheduleAll(ActionCallback_t cb)
{
    bool ret = false;
    ListLock();
    uint8_t currentCursor = mNodeStartIdx;
    uint8_t nextCursor = currentCursor;
    bool isEnd;
    do{
        currentCursor = nextCursor;
        nextCursor = mNodes[currentCursor].nextNodeIdx;
        isEnd = currentCursor == mNodeEndIdx;
        if (mNodes[currentCursor].callback == cb)
        {
            ret = true;
            removeNodeAt(currentCursor);
        }
    } while (!isEnd);
    ListUnlock();
    return ret;
}

void ActionScheduler_Clear()
{
    ListLock();
    for (uint16_t i = 0; i < MAX_ACTION_SCHEDULER_NODES; i++)
    {
        mNodes[i].usedCounter = 0U;
        mNodes[i].arg = NULL;
        mNodes[i].callback = NULL;
        mNodes[i].delayToPrevious = 0U;
        mNodes[i].reload = 0U;
        mNodes[i].nextNodeIdx = 0U;
        mNodes[i].previousNodeIdx = 0U;
    }
    mNodeStartIdx = 0;
    mNodeEndIdx = 0;
    mActiveNodes = 0;
    mProceedingTime = 0;
    ListUnlock();
}

uint32_t ActionScheduler_GetNextEventDelay(void)
{
    if(mActiveNodes > 0U)
    {
        return mNodes[mNodeStartIdx].delayToPrevious;
    }
    return UINT32_MAX;
}

// It is possible that scheduling is from a ActionScheduler callback, in this case we need to know how much time it is proceeding in the middle for precise time control to schedule new event
uint32_t ActionScheduler_GetProceedingTime()
{
    return mProceedingTime;
}

void ActionScheduler_ClearProceedingTime()
{
    mProceedingTime = 0;
}

bool ActionScheduler_IsCallbackArmed(ActionCallback_t cb)
{
    if (mNodes[mNodeStartIdx].callback == cb)
    {
        return true;
    }
    for (uint8_t i = (mNodeStartIdx + 1U) % MAX_ACTION_SCHEDULER_NODES; i != mNodeStartIdx; i = (i + 1U) % MAX_ACTION_SCHEDULER_NODES)
    {
        if (mNodes[i].callback == cb)
        {
            return true;
        }
    }
    return false;
}
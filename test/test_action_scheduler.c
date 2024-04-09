#include "action_scheduler.h"
#include "unity.h"

void Enter_Critical() {}
void Exit_Critical() {}
void setUp(void) {}    /* Is run before every test, put unit init calls here. */
void tearDown(void) {} /* Is run after every test, put unit clean-up calls here. */
// Callback functions for testing
static ActionReturn_t callback1(void *arg)
{
    return ACTION_ONESHOT;
}

static ActionReturn_t callback2(void *arg)
{
    return ACTION_RELOAD;
}

void test_ActionScheduler_Schedule()
{
    ActionScheduler_Clear();
    ActionSchedulerId_t id1 = ActionScheduler_Schedule(100, callback1, NULL);
    ActionSchedulerId_t id2 = ActionScheduler_Schedule(200, callback2, NULL);
    TEST_ASSERT_NOT_EQUAL(ACTION_SCHEDULER_ID_INVALID, id1);
    TEST_ASSERT_NOT_EQUAL(ACTION_SCHEDULER_ID_INVALID, id2);
    TEST_ASSERT_EQUAL_UINT32(100, ActionScheduler_GetNextEventDelay());

    ActionScheduler_Proceed(100);
    TEST_ASSERT_EQUAL_UINT32(100, ActionScheduler_GetNextEventDelay());

    ActionScheduler_Proceed(100);
    TEST_ASSERT_EQUAL_UINT32(200, ActionScheduler_GetNextEventDelay());
}

void test_ActionScheduler_UnscheduleById()
{
    ActionScheduler_Clear();
    ActionSchedulerId_t id1 = ActionScheduler_Schedule(100, callback1, NULL);
    ActionSchedulerId_t id2 = ActionScheduler_Schedule(200, callback2, NULL);
    TEST_ASSERT_TRUE(ActionScheduler_UnscheduleById(&id1));
    TEST_ASSERT_EQUAL_UINT32(200, ActionScheduler_GetNextEventDelay());
    TEST_ASSERT_TRUE(ActionScheduler_UnscheduleById(&id2));
    id1 = ActionScheduler_Schedule(100, callback1, NULL);
    id1 = id1 & 0x0f; // Clear the used counter number
    TEST_ASSERT_FALSE(ActionScheduler_UnscheduleById(&id1));
    TEST_ASSERT_EQUAL_UINT32(100, ActionScheduler_GetNextEventDelay());
}

void test_ActionScheduler_UnscheduleByCallback()
{
    ActionScheduler_Clear();
    ActionScheduler_Schedule(100, callback1, NULL);
    ActionScheduler_Schedule(200, callback2, NULL);
    TEST_ASSERT_TRUE(ActionScheduler_UnscheduleByCallback(callback1));
    TEST_ASSERT_EQUAL_UINT32(200, ActionScheduler_GetNextEventDelay());
}

void test_ActionScheduler_ScheduleWithReload()
{
    ActionScheduler_Clear();
    ActionSchedulerId_t id1 = ActionScheduler_ScheduleWithReload(100, 300, callback2, NULL);
    TEST_ASSERT_NOT_EQUAL(ACTION_SCHEDULER_ID_INVALID, id1);
    TEST_ASSERT_EQUAL_UINT32(100, ActionScheduler_GetNextEventDelay());

    ActionScheduler_Proceed(100);
    TEST_ASSERT_EQUAL_UINT32(300, ActionScheduler_GetNextEventDelay());
}

void test_ActionScheduler_GetProceedingTime()
{
    ActionScheduler_Clear();
    ActionScheduler_ClearProceedingTime();
    ActionScheduler_Schedule(100, callback1, NULL);
    ActionScheduler_Proceed(50);
    TEST_ASSERT_EQUAL_UINT32(50, ActionScheduler_GetProceedingTime());
    ActionScheduler_Proceed(50);
    TEST_ASSERT_EQUAL_UINT32(100, ActionScheduler_GetProceedingTime());
    ActionScheduler_ClearProceedingTime();
    TEST_ASSERT_EQUAL_UINT32(0, ActionScheduler_GetProceedingTime());
}

void test_ActionScheduler_IsCallbackArmed()
{
    ActionScheduler_Clear();
    ActionScheduler_Schedule(100, callback1, NULL);
    ActionScheduler_Schedule(200, callback2, NULL);
    TEST_ASSERT_TRUE(ActionScheduler_IsCallbackArmed(callback1));
    TEST_ASSERT_TRUE(ActionScheduler_IsCallbackArmed(callback2));
    ActionScheduler_Proceed(100);
    TEST_ASSERT_FALSE(ActionScheduler_IsCallbackArmed(callback1));
    TEST_ASSERT_TRUE(ActionScheduler_IsCallbackArmed(callback2));
    ActionScheduler_Proceed(300);
    TEST_ASSERT_TRUE(ActionScheduler_IsCallbackArmed(callback2));
}

#define NUM_CALLBACKS 64

static int callbacksExecuted[NUM_CALLBACKS] = {0};

static ActionReturn_t callback(void *arg)
{
    int index = (int)arg;
    callbacksExecuted[index]++;
    return ACTION_ONESHOT;
}

void test_ActionScheduler_LargeNumberOfCallbacks()
{
    ActionScheduler_Clear();

    // Schedule a large number of callbacks
    ActionSchedulerId_t ids[NUM_CALLBACKS];
    for (int i = 0; i < NUM_CALLBACKS; i++)
    {
        ids[i] = ActionScheduler_Schedule(i * 10 + 1, callback, (void *)i);
        TEST_ASSERT_NOT_EQUAL(ACTION_SCHEDULER_ID_INVALID, ids[i]);
    }

    // Proceed with time and verify that callbacks are fired correctly
    for (int i = 0; i < NUM_CALLBACKS; i++)
    {
        ActionScheduler_Proceed(10);
        for (int j = 0; j <= i; j++)
        {
            TEST_ASSERT_EQUAL(1, callbacksExecuted[j]);
        }
        for (int j = i + 1; j < NUM_CALLBACKS; j++)
        {
            TEST_ASSERT_EQUAL(0, callbacksExecuted[j]);
        }
    }

    // Unschedule remaining callbacks
    for (int i = 0; i < NUM_CALLBACKS; i++)
    {
        ActionScheduler_UnscheduleById(&ids[i]);
    }
}

void test_ActionScheduler_UnscheduleFinishedAction()
{
    ActionScheduler_Clear();
    ActionSchedulerId_t id1 = ActionScheduler_Schedule(100, callback1, NULL);
    TEST_ASSERT_TRUE(ActionScheduler_Proceed(100));
    TEST_ASSERT_FALSE(ActionScheduler_UnscheduleById(&id1));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_ActionScheduler_Schedule);
    RUN_TEST(test_ActionScheduler_UnscheduleById);
    RUN_TEST(test_ActionScheduler_UnscheduleByCallback);
    RUN_TEST(test_ActionScheduler_ScheduleWithReload);
    RUN_TEST(test_ActionScheduler_GetProceedingTime);
    RUN_TEST(test_ActionScheduler_IsCallbackArmed);
    RUN_TEST(test_ActionScheduler_LargeNumberOfCallbacks);
    RUN_TEST(test_ActionScheduler_UnscheduleFinishedAction);
    return UNITY_END();
}
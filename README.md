# ActionScheduler
Library for delayed function call
Would compile under Arm Gcc
critical_section.h is for arm interrupt safety, so you can call schedule function under interrupt context

Quick start(STM32):

```
#include "action_scheduler.h"
#include <stdint.h>
#include "stm32g0xx_hal.h"	// your stm32 variant HAL head

ActionReturn_t myOneShotFun(void* arg)
{
	printf("This is one shot function %d\n", (int)arg);
	return ACTION_ONESHOT;
}

ActionReturn_t myPeriodicFun(void* arg)
{
	printf("This is period function %d\n", (int)arg);
	return ACTION_RELOAD;
}

void main()
{
	static uint32_t lastHalTick = 0;
	ActionScheduler_Clear();
	ActionScheduler_Schedule(1000, myOneShotFun, 1);
	ActionScheduler_Schedule(2000, myPeriodicFun, 2);
	
	while(true)
	{
		uint32_t haltick = HAL_GetTick();
		ActionScheduler_Proceed(haltick - lastHalTick);
		lastHalTick = haltick;
		// Turn on these code to enter low power mode between the scheduled events, and use RTC to wake up(you should enable RTC in your code)
		// Otherwise, leave it unchanged for simpliest example code
		/*
		uint32_t nextEventDelay = ActionScheduler_GetNextEventDelay();
		if(nextEventDelay >= 1000)
		{
			HAL_SuspendTick();
			HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, nextEventDelay/1000 - 1, RTC_WAKEUPCLOCK_CK_SPRE_16BITS);
			HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
			HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
			HAL_ResumeTick();
			// Stop1 mode stops PLL, we need to re-initialize the clock
			SystemClock_Config();
			// systick pauses during stop mode, the simpliest way is to push lastHalTick back. 
			// This is a very rough example here for demo only. Better way is to use RTC tick instead
			lastHalTick -= nextEventDelay;
		}
		*/
	}
}
```
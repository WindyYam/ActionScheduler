# ActionScheduler
ActionScheduler is a lightweight scheduling library for delayed function execution with an argument(can be a pointer to real data), designed for embedded systems and real-time applications. It allows you to schedule one-shot or recurring function calls with precise timing, making it useful for a variety of tasks such as timeouts, state machine transitions, and periodic sensor readings.  

The key design is to eliminate the need for declaring any struct/any registration, only the callback function itself. You are to schedule callback function directly.  
All the function events are managed in a linked list as timeline, allowing easy insertion/removal.  
Specially, you can also treat it as a queue. In this case, the argument is the queue data, the function is the function to process queue data, the delay will be 0 as we want immediate processing.  
In multithreads environment, you can also treat it as a way to synchronize function call(like a queue).  

## Key Features

- **Efficient Scheduling**: ActionScheduler uses a lightweight, low-overhead design to manage scheduled actions, ensuring minimal impact on system performance. The timeline is managed as a linked list.  
- **Flexible Scheduling**: You can schedule both one-shot and recurring actions, with the ability to specify the delay and, for recurring actions, the reload interval.  
- **Callback-based Design**: Actions are defined using function pointers, allowing you to schedule any arbitrary function as an action.  
- **Cross-Platform Compatibility**: The library is written in standard C99 and can be used on a variety of embedded platforms, including ARM Cortex-M microcontrollers.  
- **Interrupt-Safe**: The library is designed to be interrupt-safe, allowing you to schedule actions from within interrupt contexts.  
- **Flexible Time Keeping**: The library does not depend on any specific time-keeping mechanism, allowing you to integrate it with your application's existing time management system.  
- **Low Power Friendly**: The library is easy to be incorporated with low power strategy as events are scheduled as timeline nodes. You can get the time remaining to the next event that's going to happen.  

## Usage
To use the ActionScheduler library, follow these steps:  

Include the action_scheduler.h header file in your project.  
Implement your action callback functions, which should have the following signature:  
`ActionReturn_t myActionCallback(void* arg);`  
The callback function should return `ACTION_ONESHOT` if the action runs only one time, or `ACTION_RELOAD` if the action should be scheduled again after the specified reload interval.  
Schedule actions using the provided functions, such as `ActionScheduler_Schedule()` and `ActionScheduler_ScheduleReload()`.  
Periodically call `ActionScheduler_Proceed(timeElapsed)` to update the scheduler and execute any scheduled actions.  

See the provided example code for a complete demonstration of how to use the ActionScheduler library.  
Configuration
The ActionScheduler library can be configured by defining the following preprocessor macro before including the header file:  

`MAX_ACTION_SCHEDULER_NODES`: Specifies the maximum number of scheduled actions the library can handle. The default value is 64, but you can adjust this based on the requirements of your application.  

Here is an example:
```
#include "action_scheduler.h"
#include <stdint.h>
#include "stm32g0xx_hal.h"    // your stm32 variant HAL head

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
    }
}
```

## Contribution and Feedback

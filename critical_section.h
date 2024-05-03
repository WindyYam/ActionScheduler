#ifndef __CRITICAL_SECTION_H
#define __CRITICAL_SECTION_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined ( __CC_ARM ) || defined (__ARMCC_VERSION) || (defined (__arm__) && defined ( __GNUC__ )) || defined ( __ICCARM__ ) || defined ( __TI_ARM__ )
#include <cmsis_compiler.h>
__STATIC_FORCEINLINE uint32_t Enter_Critical()
{
    uint32_t primask_bit = __get_PRIMASK();
    __disable_irq();
    return primask_bit;
}
__STATIC_FORCEINLINE void Exit_Critical(uint32_t primask_bit)
{
    __set_PRIMASK(primask_bit);
}
#else
// For other architectures, implement these 2 functions by yourself somewhere in the code, for interrupt context safety
// Or you can put these 2 functions as empty, if you don't care about scheduling from ISR
uint32_t Enter_Critical();
void Exit_Critical(uint32_t primask_bit);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __CRITICAL_SECTION_H */

#ifndef __WAKE_LOCK_H
#define __WAKE_LOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_framework.h"

#ifdef USE_WAKE_LOCK
// For every C/C++ source file which include this header, module_wake_lock static variable will be created, to describe the lock used on that module
// In general, ACQUIRE_WAKELOCK() and RELEASE_WAKELOCK() should be appeared in pairs in the same file, not from another file function, as the module_wake_lock variable will be different
static bool module_wake_lock = false;

#define ACQUIRE_WAKELOCK() {   if(!module_wake_lock){\
                                    module_wake_lock = true;\
                                    DEBUG_PRINTF("%s():", __FUNCTION__);\
                                    AppFramework_WakeLockRecursive(true);\
                                }\
                                else\
                                {\
                                    DEBUG_PRINTF("Acquire WakeLock Error! %s\n", __FUNCTION__);\
                                }\
                            }
#define RELEASE_WAKELOCK() {   if(module_wake_lock){\
                                    DEBUG_PRINTF("%s():", __FUNCTION__);\
                                    AppFramework_WakeLockRecursive(false);\
                                    module_wake_lock = false;\
                                }\
                                else\
                                {\
                                    DEBUG_PRINTF("Release WakeLock Error! %s\n", __FUNCTION__);\
                                }\
                            }

#define IS_WAKELOCK() module_wake_lock
#else
#define ACQUIRE_WAKELOCK()
#define RELEASE_WAKELOCK()
#define IS_WAKELOCK() false
#endif
#ifdef __cplusplus
}
#endif

#endif /* __WAKE_LOCK_H */

/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef _GDIAL_SERVICE_LOGGING_H_
#define _GDIAL_SERVICE_LOGGING_H_

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
    (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__))

typedef enum gdial_plat_util_LogLevel_enum
{
    FATAL_LEVEL = 0,
    ERROR_LEVEL,
    WARNING_LEVEL,
    INFO_LEVEL,
    VERBOSE_LEVEL,
    TRACE_LEVEL
}
gdial_plat_util_LogLevel;

#define __FILENAME__ \
        (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
        (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__))

#define _LOG(LEVEL, FORMAT, ...)          \
                gdial_plat_util_log(LEVEL,                       \
                        __func__, __FILENAME__, __LINE__, syscall(__NR_gettid), \
                        FORMAT,                          \
                        ##__VA_ARGS__)

void gdial_plat_util_logger_init(void);
void gdial_plat_util_set_loglevel(gdial_plat_util_LogLevel level);
void gdial_plat_util_log(gdial_plat_util_LogLevel level,
                        const char* func,
                        const char* file,
                        int line,
                        int threadID,
                        const char* format, ...);

#define GDIAL_LOGTRACE(FMT, ...)   _LOG(TRACE_LEVEL, FMT, ##__VA_ARGS__)
#define GDIAL_LOGVERBOSE(FMT, ...) _LOG(VERBOSE_LEVEL, FMT, ##__VA_ARGS__)
#define GDIAL_LOGINFO(FMT, ...)    _LOG(INFO_LEVEL, FMT, ##__VA_ARGS__)
#define GDIAL_LOGWARNING(FMT, ...) _LOG(WARNING_LEVEL, FMT, ##__VA_ARGS__)
#define GDIAL_LOGERROR(FMT, ...)   _LOG(ERROR_LEVEL, FMT, ##__VA_ARGS__)
#define GDIAL_LOGFATAL(FMT, ...)   _LOG(FATAL_LEVEL, FMT, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* _GDIAL_SERVICE_COMMON_H_ */
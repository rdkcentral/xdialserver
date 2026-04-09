/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

/**
 * @file xdialserver_test_stubs.cpp
 * @brief Stub implementations for external / platform helpers.
 *
 * gdial_plat_util_log is used by the GDIAL_LOG* macros in every source
 * file compiled into the test binary.  All other declarations from
 * gdialservicelogging.h that live in gdial-plat-util.c are also stubbed
 * here so that TU does not need to be compiled in.
 */

#include <stdio.h>
#include <stdarg.h>

extern "C" {
#include "gdialservicelogging.h"
}

extern "C" void gdial_plat_util_log(
        gdial_plat_util_LogLevel level,
        const char *func,
        const char *file,
        int line,
        int threadID,
        const char *format, ...)
{
    /* Suppress all log output during unit tests.
     * Flip to fprintf(stderr, ...) if you need debug output. */
    (void)level; (void)func; (void)file; (void)line; (void)threadID;
    (void)format;
}

extern "C" void gdial_plat_util_logger_init(void) {}

extern "C" void gdial_plat_util_set_loglevel(gdial_plat_util_LogLevel level)
{
    (void)level;
}


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
 * @file gdial_plat_stubs.cpp
 * @brief Placeholder stub translation unit.
 *
 * gdial-plat-app.c is now compiled directly into the test binary, providing
 * the real gdial_plat_application_* and gdial_plat_init/term implementations.
 * gdial_os_stubs.cpp provides the gdial_os_* and gdial_init/term/register_*
 * stubs that gdial-plat-app.c delegates into.
 *
 * gdial-plat-dev.c is also compiled directly into the L1 test binary, so this
 * file intentionally exports no symbols to avoid duplicate definitions.
 */

#include <glib.h>



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

#ifndef GDIAL_DEBUG_H_
#define GDIAL_DEBUG_H_

#include <glib.h>
#include "gdialservicelogging.h"

G_BEGIN_DECLS

#define GDIAL_PERF_ENABLE 1

#if GDIAL_PERF_ENABLE
#define GDIAL_PERF_TIME_BEGIN() gint64 time__begin__ = g_get_real_time();
#define GDIAL_PERF_TIME_END(avg, w)\
if ((avg) > 0) {\
  static gint64 time_hist[avg] = {0};\
  static double time_hist_total = 0;\
  static gint64 counter = 0;\
  time_hist[counter++%(avg)] = g_get_real_time() - time__begin__;\
  time_hist_total+=time_hist[(counter-1)%(avg)];\
  time_hist_total-=time_hist[counter%(avg)];\
  double average = (time_hist_total) / (counter > (avg) ? (avg) : counter);\
  GDIAL_LOGERROR("time__ used average %0.3fms for %ld requests (total=%0.3fms)\r\n", average/1000, counter, time_hist_total/1000);\
}
#else
#define GDIAL_PERF_TIME_BEGIN()
#define GDIAL_PERF_TIME_END(avg, w)
#endif

#define g_print_with_timestamp(format, ...)\
{\
  static gchar timestamp_[128] = {0};\
  time_t t;\
  t = time(NULL);\
  strftime(timestamp_, sizeof(timestamp_), "%FT%T", localtime(&t));\
  GDIAL_LOGINFO("[%s] "format, timestamp_, __VA_ARGS__);\
}

#define g_warn_msg_if_fail(expr, format, ...)\
do {\
  if G_LIKELY (expr) ;\
  else {\
    GString *msg_buf = g_string_new("");\
    g_string_printf(msg_buf, "\r\nFailed Condition: [%s] - Error Message: "format, #expr, __VA_ARGS__);\
    gchar *msg = g_string_free(msg_buf, FALSE);\
    GDIAL_LOGWARNING("%s", msg); /*g_warn_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, msg);*/ \
    g_free(msg);\
  }\
} while (0)

G_END_DECLS
#endif

/*
 * If not stated otherwise in this file or this component's LICENSE file the
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "gdial-plat-util.h"
#include "gdialservicelogging.h"

const char * gdial_plat_util_get_iface_ipv4_addr(const char *ifname) {
  const char *result = NULL;
  static char iface_ipv4_addr[INET_ADDRSTRLEN] = {'\0'};
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock >= 0) {
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';
    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
      if (inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, iface_ipv4_addr, INET_ADDRSTRLEN)) {
        result = iface_ipv4_addr;
      }
    }
    close(sock);
  }

  return result;
}

const char * gdial_plat_util_get_iface_mac_addr(const char *ifname) {
  #define GDIAL_HW_ADDRSTRLEN 18
  const char *result = NULL;
  static char iface_mac_addr[GDIAL_HW_ADDRSTRLEN] = "00:00:00:00:00:00";
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  int sock = socket(AF_INET,SOCK_DGRAM,0);
  if (sock >= 0) {
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';
    if (ioctl( sock, SIOCGIFHWADDR, &ifr ) == 0) {
      unsigned char *sa_data = (unsigned char *)ifr.ifr_hwaddr.sa_data;
      sprintf(iface_mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x", sa_data[0], sa_data[1], sa_data[2], sa_data[3], sa_data[4], sa_data[5]);
      result = iface_mac_addr;
    }
    close(sock);
  }

  return result;
}

static inline void sync_stdout()
{
    if (getenv("SYNC_STDOUT"))
        setvbuf(stdout, NULL, _IOLBF, 0);
}

static int gDefaultLogLevel = INFO_LEVEL;

void gdial_plat_util_logger_init(void)
{
    const char *level = getenv("GDIAL_LIBRARY_DEFAULT_LOG_LEVEL");

    sync_stdout();

    if (level)
    {
        gDefaultLogLevel = (atoi(level));
    }
}

void gdial_plat_util_set_loglevel(gdial_plat_util_LogLevel level)
{
    gDefaultLogLevel = level;
}

void gdial_plat_util_log(gdial_plat_util_LogLevel level,
                        const char *func,
                        const char *file,
                        int line,
                        int threadID,
                        const char *format, ...)
{
    const char *levelMap[] = {"FATAL", "ERROR", "WARN", "INFO", "VERBOSE", "TRACE"};
    const short kFormatMessageSize = 4096;
    char formatted[kFormatMessageSize];
    char file_str[1024];

    if (((FATAL_LEVEL != level)&&(ERROR_LEVEL != level))&&
        (gDefaultLogLevel < level)){
        return;
    }

    va_list argptr;
    va_start(argptr, format);
    vsnprintf(formatted, kFormatMessageSize, format, argptr);
    va_end(argptr);
    /*Fix coverity error RW.EXPR_NOT_STRUCT_OR_UNION */
    strncpy(file_str, file, sizeof(file_str));
    file_str[sizeof(file_str) - 1] = '\0';
    fprintf(stderr, "[GDIAL][%d] %s [%s:%d] %s: %s \n",
                (int)syscall(SYS_gettid),
                levelMap[level],
                basename(file_str),
                line,
                func,
                formatted);
    fflush(stderr);
}

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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "gdial-plat-util.h"

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

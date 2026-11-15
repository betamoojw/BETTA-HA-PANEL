/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdint.h>
#define LWIP_IPV6 0
#define AF_INET 2
typedef unsigned socklen_t;
struct in_addr { uint32_t s_addr; };
struct sockaddr { unsigned short sa_family; };
struct sockaddr_in { unsigned short sin_family; unsigned short sin_port; struct in_addr sin_addr; };
struct sockaddr_storage { unsigned short ss_family; unsigned short port; struct in_addr address; char data[128]; };
int getpeername(int fd, struct sockaddr *peer, socklen_t *size);

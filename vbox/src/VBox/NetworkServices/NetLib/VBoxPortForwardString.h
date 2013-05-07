/* $Id: VBoxPortForwardString.h 45559 2013-04-16 06:11:59Z vboxsync $ */
/** @file
 * VBoxPortForwardString 
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxPortForwardString_h___
#define ___VBoxPortForwardString_h___

#include <iprt/net.h>
#include <VBox/intnet.h>

RT_C_DECLS_BEGIN

#ifndef INET6_ADDRSTRLEN
  /* from netinet6/in6.h */
# define INET6_ADDRSTRLEN 46
#endif

#define PF_NAMELEN 64
/*
 * TBD: Here is shared implementation of parsing port-forward string 
 * of format:
 *      name:[ipv4 or ipv6 address]:host-port:[ipv4 or ipv6 guest addr]:guest port
 * 
 * This code supposed to be used in NetService and Frontend and perhaps in corresponding 
 * services.
 *
 * Note: ports are in host format.
 */

typedef struct PORTFORWARDRULE
{
    char aszPfrName[PF_NAMELEN];
    /* true if ipv6 and false otherwise */
    int fPfrIPv6;
    /* IPPROTO_{UDP,TCP} */
    int iPfrProto;
    RTNETADDRU uPfrHostAddr;
    char       aszPfrHostAddr[INET6_ADDRSTRLEN];
    uint16_t   u16PfrHostPort;
    RTNETADDRU uPfrGuestAddr;
    char       aszPfrGuestAddr[INET6_ADDRSTRLEN];
    uint16_t   u16PfrGuestPort;
} PORTFORWARDRULE, *PPORTFORWARDRULE;

int netPfStrToPf(const char *pcszStrPortForward,int fIPv6, PPORTFORWARDRULE pPfr);

RT_C_DECLS_END

#endif


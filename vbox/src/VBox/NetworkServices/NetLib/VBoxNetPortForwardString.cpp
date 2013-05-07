/* $Id: VBoxNetPortForwardString.cpp 45559 2013-04-16 06:11:59Z vboxsync $ */
/** @file
 * VBoxNetPortForwardString - Routines for managing port-forward strings.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/

#include <netinet/in.h>

#include <iprt/cdefs.h>
#define IPv6
#include <iprt/cidr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/net.h>
#include <iprt/getopt.h>
#include <iprt/ctype.h>


#include <VBox/log.h>

#include "VBoxPortForwardString.h"


#define PF_FIELD_SEPARATOR ':'
#define PF_ADDRESS_FIELD_STARTS '['
#define PF_ADDRESS_FIELD_ENDS ']'

#define PF_STR_FIELD_SEPARATOR ":"
#define PF_STR_ADDRESS_FIELD_STARTS "["
#define PF_STR_ADDRESS_FIELD_ENDS "]"

static int netPfStrAddressParse(char *pszRaw, int cbRaw, 
                                char *pszAddress, int cbAddress,
                                bool fEmptyAcceptable)
{
    int idxRaw = 0;
    int cbField = 0;

    AssertPtrReturn(pszRaw, -1);
    AssertPtrReturn(pszAddress, -1);
    AssertReturn(pszRaw[0] == PF_ADDRESS_FIELD_STARTS, -1);

    if (pszRaw[0] == PF_ADDRESS_FIELD_STARTS)
    {
        /* shift pszRaw to next symbol */
        pszRaw++;
        cbRaw--;
        

        /* we shouldn't face with ending here */
        AssertReturn(cbRaw > 0, VERR_INVALID_PARAMETER);

        char *pszEndOfAddress = RTStrStr(pszRaw, PF_STR_ADDRESS_FIELD_ENDS);
        
        /* no pair closing sign */
        AssertPtrReturn(pszEndOfAddress, VERR_INVALID_PARAMETER);
                        
        cbField = pszEndOfAddress - pszRaw;
        
        /* field should be less then the rest of the string */
        AssertReturn(cbField < cbRaw, VERR_INVALID_PARAMETER);
        
        if (cbField != 0)
            RTStrCopy(pszAddress, RT_MIN(cbField + 1, cbAddress), pszRaw);
        else if (!fEmptyAcceptable)
            return -1;
    }

    AssertReturn(pszRaw[cbField] == PF_ADDRESS_FIELD_ENDS, -1);

    return cbField + 2; /* length of the field and closing braces */ 
}


static int netPfStrPortParse(char *pszRaw, int cbRaw, uint16_t *pu16Port)
{
    char *pszEndOfPort = NULL;
    uint16_t u16Port = 0;
    int idxRaw = 1; /* we increment pszRaw after checks. */
    int cbRest = 0;
    size_t cbPort = 0;

    AssertPtrReturn(pszRaw, -1);
    AssertPtrReturn(pu16Port, -1);
    AssertReturn(pszRaw[0] == PF_FIELD_SEPARATOR, -1);

    pszRaw++; /* skip line separator */
    cbRaw --;

    pszEndOfPort = RTStrStr(pszRaw, ":");
    if (!pszEndOfPort)
    {
        cbRest = strlen(pszRaw);

        Assert(cbRaw == cbRest); 
        
        /* XXX: Assumption that if string is too big, it will be reported by
         * RTStrToUint16.
         */
        if (cbRest > 0)
        {
            pszEndOfPort = pszRaw + cbRest;
            cbPort = cbRest;
        }
        else
            return -1;
    }
    else
        cbPort = pszEndOfPort - pszRaw;


    idxRaw += cbPort;

    Assert(cbRest || pszRaw[idxRaw - 1] == PF_FIELD_SEPARATOR); /* we are 1 char ahead */

    char aszPort[10];
    RT_ZERO(aszPort);

    Assert(idxRaw > 0);
    RTStrCopy(aszPort, RT_MIN(RT_ELEMENTS(aszPort), (size_t)(cbPort) + 1), pszRaw);

    if (!(u16Port = RTStrToUInt16(aszPort)))
        return -1;
    
    *pu16Port = u16Port;

     return idxRaw;
}


static int netPfStrAddressPortPairParse(char *pszRaw, int cbRaw, 
                                        char *pszAddress, int cbAddress, 
                                        bool fEmptyAddressAcceptable,
                                        uint16_t *pu16Port)
{
    int idxRaw = 0;
    int idxRawTotal = 0;

    AssertPtrReturn(pszRaw, -1);
    AssertPtrReturn(pszAddress, -1);
    AssertPtrReturn(pu16Port, -2);

    /* XXX: Here we should check 0 - ':' and 1 - '[' */
    Assert(   pszRaw[0] == PF_FIELD_SEPARATOR
              && pszRaw[1] == PF_ADDRESS_FIELD_STARTS);

    pszRaw++; /* field separator skip */
    cbRaw--;
    AssertReturn(cbRaw > 0, VERR_INVALID_PARAMETER);

    idxRaw = 0;

    if (pszRaw[0] == PF_ADDRESS_FIELD_STARTS)
    {
        idxRaw += netPfStrAddressParse(pszRaw, 
                                       cbRaw - idxRaw, 
                                       pszAddress,
                                       cbAddress,
                                       fEmptyAddressAcceptable);
        if (idxRaw == -1)
            return -1;

        Assert(pszRaw[idxRaw] == PF_FIELD_SEPARATOR);
    }
    else return -1;
                    
    pszRaw += idxRaw;
    idxRawTotal += idxRaw;
    cbRaw -= idxRaw;

    AssertReturn(cbRaw > 0, VERR_INVALID_PARAMETER);
    
    idxRaw = 0;

    Assert(pszRaw[0] == PF_FIELD_SEPARATOR);

    if (pszRaw[0] == PF_FIELD_SEPARATOR)
    {
        idxRaw = netPfStrPortParse(pszRaw, strlen(pszRaw), pu16Port);

        Assert(strlen(&pszRaw[idxRaw]) == 0 || pszRaw[idxRaw] == PF_FIELD_SEPARATOR);

        if (idxRaw == -1)
            return -2;

        idxRawTotal += idxRaw;

        return idxRawTotal + 1;
    }
    else return -1; /* trailing garbage in the address */
}

/* XXX: Having fIPv6 we might emprove adress verification comparing address length 
 * with INET[6]_ADDRLEN
 */
int netPfStrToPf(const char *pcszStrPortForward, int fIPv6, PPORTFORWARDRULE pPfr)
{
    char *pszName;
    int  proto;
    char *pszHostAddr;
    char *pszGuestAddr;
    uint16_t u16HostPort;
    uint16_t u16GuestPort;
    bool fTcpProto = false;

    char *pszRawBegin = NULL; 
    char *pszRaw = NULL;
    int idxRaw = 0;
    int cbToken = 0;
    int cbRaw = 0;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(pcszStrPortForward, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pPfr, VERR_INVALID_PARAMETER);

    memset(pPfr, 0, sizeof(PORTFORWARDRULE));

    pszHostAddr = &pPfr->aszPfrHostAddr[0];
    pszGuestAddr = &pPfr->aszPfrGuestAddr[0];
    pszName = &pPfr->aszPfrName[0];

    cbRaw = strlen(pcszStrPortForward); 

    /* Minimal rule ":tcp:[]:0:[]:0" has got lenght 14 */
    AssertReturn(cbRaw > 14, VERR_INVALID_PARAMETER);
    
    pszRaw = RTStrDup(pcszStrPortForward);

    AssertPtrReturn(pszRaw, VERR_NO_MEMORY);

    pszRawBegin = pszRaw;

    /* name */
    if (pszRaw[idxRaw] == PF_FIELD_SEPARATOR) 
        idxRaw = 1; /* begin of the next segment */
    else
    {
        char *pszEndOfName = RTStrStr(pszRaw + 1, PF_STR_FIELD_SEPARATOR);
        if (!pszEndOfName)
            goto invalid_parameter;

        cbToken = (pszEndOfName) - pszRaw; /* don't take : into account */
        /* XXX it's unacceptable to have only name entry in PF */
        AssertReturn(cbToken < cbRaw, VERR_INVALID_PARAMETER);
        
        if (   cbToken < 0
            || (size_t)cbToken >= PF_NAMELEN)
            goto invalid_parameter;

        RTStrCopy(pszName, 
                  RT_MIN((size_t)cbToken + 1, PF_NAMELEN), 
                  pszRaw);
        pszRaw += cbToken; /* move to separator */
    }
    
    AssertReturn(pszRaw[0] == PF_FIELD_SEPARATOR, VERR_INVALID_PARAMETER);
    /* protocol */

    pszRaw++; /* skip separator */
    idxRaw = 0;
    
    cbRaw--;

    if (     ((    fTcpProto = (RTStrNICmp(pszRaw, "tcp", 3) == 0)
              ||  (RTStrNICmp(pszRaw, "udp", 3) == 0))
          && (pszRaw[3] == PF_FIELD_SEPARATOR)))
    {
        proto = (fTcpProto ? IPPROTO_TCP : IPPROTO_UDP);
        idxRaw = 3;
    }
    else
        goto invalid_parameter;
                    
    pszRaw += idxRaw;
    cbRaw -= idxRaw;
    idxRaw = 0;
                    
    idxRaw = netPfStrAddressPortPairParse(pszRaw, cbRaw, 
                                         pszHostAddr, INET6_ADDRSTRLEN,
                                         true, &u16HostPort);
    if (idxRaw < 0)
        return VERR_INVALID_PARAMETER;

    pszRaw += idxRaw;
    cbRaw -= idxRaw;

    Assert(pszRaw[0] == PF_FIELD_SEPARATOR);

    idxRaw = 0;

    idxRaw = netPfStrAddressPortPairParse(pszRaw, cbRaw, 
                                          pszGuestAddr, 
                                          INET6_ADDRSTRLEN,
                                          false,
                                          &u16GuestPort);

    if (idxRaw < 0)
        goto invalid_parameter;

    /* XXX: fill the rule */
    pPfr->fPfrIPv6 = fIPv6;
    pPfr->iPfrProto = proto;

    
    if (strlen(pszHostAddr))
    {
        if (!fIPv6)
            rc = RTNetStrToIPv4Addr(pszHostAddr, &pPfr->uPfrHostAddr.IPv4);
#if 0 /* No IPv6 yet */
        else
            rc = RTNetStrToIPv6Addr(pszHostAddr, &pPfr->uPfrHostAddr.IPv6);
#endif

        if (RT_FAILURE(rc))
            goto invalid_parameter;
    }
    
    pPfr->u16PfrHostPort = u16HostPort;

    if (strlen(pszGuestAddr))
    {
        if (!fIPv6)
            rc = RTNetStrToIPv4Addr(pszGuestAddr, &pPfr->uPfrGuestAddr.IPv4);       
#if 0
        else
            rc = RTNetStrToIPv6Addr(pszGuestAddr, &pPfr->uPfrGuestAddr.IPv6);
#endif
        if (RT_FAILURE(rc))
            goto invalid_parameter;
    }
    else
        goto invalid_parameter; /* guest address should be defined */
    
    pPfr->u16PfrGuestPort = u16GuestPort;
    
    Log(("name: %s\n"
         "proto: %d\n"
         "host address: %s\n"
         "host port: %d\n"
         "guest address: %s\n"
         "guest port:%d\n",
         pszName, proto,
         pszHostAddr, u16HostPort,
         pszGuestAddr, u16GuestPort));

    RTStrFree(pszRawBegin);
    return VINF_SUCCESS;
    
invalid_parameter:
    RTStrFree(pszRawBegin);
    if (pPfr)
        memset(pPfr, 0, sizeof(PORTFORWARDRULE));
    return VERR_INVALID_PARAMETER;
}

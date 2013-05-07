/* $Id: tstNtQueryStuff.cpp 45161 2013-03-25 11:08:50Z vboxsync $ */
/** @file
 * SUP Testcase - Exploring some NT Query APIs.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <Windows.h>
#include <winternl.h>

typedef enum
{
    MemoryBasicInformation = 0,
    MemoryWorkingSetList,
    MemorySectionName,
    MemoryBasicVlmInformation
} MEMORY_INFORMATION_CLASS;

typedef struct
{
    UNICODE_STRING  SectionFileName;
    WCHAR           NameBuffer[ANYSIZE_ARRAY];
} MEMORY_SECTION_NAME;

extern "C"
NTSYSAPI NTSTATUS NTAPI NtQueryVirtualMemory(IN HANDLE hProcess,
                                             IN LPCVOID pvWhere,
                                             IN MEMORY_INFORMATION_CLASS MemoryInfo,
                                             OUT PVOID pvBuf,
                                             IN SIZE_T cbBuf,
                                             OUT PSIZE_T pcbReturned OPTIONAL);

#define ProcessDebugPort            ((PROCESSINFOCLASS)7 )
#define ProcessHandleCount          ((PROCESSINFOCLASS)20)
#define ProcessWow64Information     ((PROCESSINFOCLASS)26)
#define ProcessImageFileName        ((PROCESSINFOCLASS)27)
#define ProcessDebugObjectHandle    ((PROCESSINFOCLASS)30)
#define ProcessExecuteFlags         ((PROCESSINFOCLASS)34)
#define ProcessImageFileNameWin32   ((PROCESSINFOCLASS)43)
#define ProcessImageFileMapping     ((PROCESSINFOCLASS)44)


#include <iprt/test.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct FLAGDESC
{
    ULONG       f;
    const char *psz;
} FLAGDESC;
typedef const FLAGDESC *PCFLAGDESC;



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTTEST g_hTest = NIL_RTTEST;
static HANDLE g_hProcess = NULL;


static char *stringifyAppend(char *pszBuf, size_t *pcbBuf, const char *pszAppend, bool fWithSpace)
{
    size_t cchAppend = strlen(pszAppend);
    if (cchAppend + 1 + fWithSpace <= *pcbBuf)
    {
        if (fWithSpace)
        {
            *pszBuf++ = ' ';
            *pcbBuf  += 1;
        }
        memcpy(pszBuf, pszAppend, cchAppend + 1);
        *pcbBuf -= cchAppend;
        pszBuf  += cchAppend;
    }

    return pszBuf;
}


static char *stringifyAppendUnknownFlags(uint32_t fFlags, char *pszBuf, size_t *pcbBuf, bool fWithSpace)
{
    for (unsigned iBit = 0; iBit < 32; iBit++)
        if (fFlags & RT_BIT_32(iBit))
        {
            char szTmp[32];             /* lazy bird */
            RTStrPrintf(szTmp, sizeof(szTmp), "BIT(%d)", iBit);
            pszBuf = stringifyAppend(pszBuf, pcbBuf, szTmp, fWithSpace);
            fWithSpace = true;
        }

    return pszBuf;
}


static char *stringifyFlags(uint32_t fFlags, char *pszBuf, size_t cbBuf, PCFLAGDESC paFlagDesc, size_t cFlagDesc)
{
    char *pszBufStart = pszBuf;
    if (fFlags)
    {
        for (size_t i = 0; i < cFlagDesc; i++)
        {
            if (fFlags & paFlagDesc[i].f)
            {
                fFlags &= ~paFlagDesc[i].f;
                pszBuf = stringifyAppend(pszBuf, &cbBuf, paFlagDesc[i].psz, pszBuf != pszBufStart);
            }
        }

        if (fFlags)
            stringifyAppendUnknownFlags(fFlags, pszBuf, &cbBuf, pszBuf != pszBufStart);
    }
    else
    {
        pszBuf[0] = '0';
        pszBuf[1] = '\0';
    }
    return pszBufStart;
}


static char *stringifyMemType(uint32_t fType, char *pszBuf, size_t cbBuf)
{
    static const FLAGDESC s_aMemTypes[] =
    {
        { MEM_PRIVATE,      "PRIVATE" },
        { MEM_MAPPED,       "MAPPED" },
        { MEM_IMAGE,        "IMAGE" },
    };
    return stringifyFlags(fType, pszBuf, cbBuf, s_aMemTypes, RT_ELEMENTS(s_aMemTypes));
}


static char *stringifyMemState(uint32_t fState, char *pszBuf, size_t cbBuf)
{
    static const FLAGDESC s_aMemStates[] =
    {
        { MEM_FREE,         "FREE" },
        { MEM_COMMIT,       "COMMIT" },
        { MEM_RESERVE,      "RESERVE" },
        { MEM_DECOMMIT,     "DECOMMMIT" },
    };
    return stringifyFlags(fState, pszBuf, cbBuf, s_aMemStates, RT_ELEMENTS(s_aMemStates));
}


static char *stringifyMemProt(uint32_t fProt, char *pszBuf, size_t cbBuf)
{
    static const FLAGDESC s_aProtections[] =
    {
        { PAGE_NOACCESS,            "NOACCESS" },
        { PAGE_READONLY,            "READONLY" },
        { PAGE_READWRITE,           "READWRITE" },
        { PAGE_WRITECOPY,           "WRITECOPY" },
        { PAGE_EXECUTE,             "EXECUTE" },
        { PAGE_EXECUTE_READ,        "EXECUTE_READ" },
        { PAGE_EXECUTE_READWRITE,   "EXECUTE_READWRITE" },
        { PAGE_EXECUTE_WRITECOPY,   "EXECUTE_WRITECOPY" },
        { PAGE_GUARD,               "GUARD" },
        { PAGE_NOCACHE,             "NOCACHE" },
        { PAGE_WRITECOMBINE,        "WRITECOMBINE" },

    };
    return stringifyFlags(fProt, pszBuf, cbBuf, s_aProtections, RT_ELEMENTS(s_aProtections));
}



static void tstQueryVirtualMemory(void)
{
    RTTestISub("NtQueryVirtualMemory");

    uintptr_t   cbAdvance = 0;
    uintptr_t   uPtrWhere = 0;
    for (;;)
    {
        SIZE_T                      cbActual = 0;
        MEMORY_BASIC_INFORMATION    MemInfo  = { 0, 0, 0, 0, 0, 0, 0 };
        NTSTATUS rcNt = NtQueryVirtualMemory(g_hProcess,
                                             (void const *)uPtrWhere,
                                             MemoryBasicInformation,
                                             &MemInfo,
                                             sizeof(MemInfo),
                                             &cbActual);
        if (!NT_SUCCESS(rcNt))
        {
            RTTestIPrintf(RTTESTLVL_ALWAYS, "%p: rcNt=%#x\n", uPtrWhere, rcNt);
            break;
        }

        /* stringify the memory state. */
        char szMemType[1024];
        char szMemState[1024];
        char szMemProt[1024];
        char szAllocProt[1024];

        if (   MemInfo.AllocationBase != NULL
            && MemInfo.AllocationBase == MemInfo.BaseAddress
            && MemInfo.Protect        != MemInfo.AllocationProtect)
            RTTestIPrintf(RTTESTLVL_ALWAYS, "\n");

        RTTestIPrintf(RTTESTLVL_ALWAYS, "%p-%p  %-8s  %-8s  %-12s",
                      MemInfo.BaseAddress, (uintptr_t)MemInfo.BaseAddress + MemInfo.RegionSize - 1,
                      stringifyMemType(MemInfo.Type, szMemType, sizeof(szMemType)),
                      stringifyMemState(MemInfo.State, szMemState, sizeof(szMemState)),
                      stringifyMemProt(MemInfo.Protect, szMemProt, sizeof(szMemProt))
                      );
        if ((uintptr_t)MemInfo.AllocationBase != 0)
        {
            if (MemInfo.AllocationBase != MemInfo.BaseAddress)
                RTTestIPrintf(RTTESTLVL_ALWAYS, "  %p", MemInfo.AllocationBase);
            else
                RTTestIPrintf(RTTESTLVL_ALWAYS, "  %s", stringifyMemProt(MemInfo.AllocationProtect, szAllocProt, sizeof(szAllocProt)));
        }
        RTTestIPrintf(RTTESTLVL_ALWAYS, "\n");

        if ((uintptr_t)MemInfo.BaseAddress != uPtrWhere)
            RTTestIPrintf(RTTESTLVL_ALWAYS, " !Warning! Queried %p got BaseAddress=%p!\n",
                          uPtrWhere, MemInfo.BaseAddress);

        /* Image or mapped, then try get a file name. */
        if (MemInfo.Type == MEM_IMAGE || MemInfo.Type == MEM_MAPPED)
        {
            union
            {
                MEMORY_SECTION_NAME     Core;
                WCHAR                   awcPadding[UNICODE_STRING_MAX_CHARS + (sizeof(UNICODE_STRING_MAX_CHARS) + 1) / sizeof(WCHAR)];
            } uBuf;
            RT_ZERO(uBuf);
            uBuf.Core.SectionFileName.Length        = UNICODE_STRING_MAX_CHARS * 2;
            uBuf.Core.SectionFileName.MaximumLength = UNICODE_STRING_MAX_CHARS * 2;
            uBuf.Core.SectionFileName.Buffer        = &uBuf.Core.NameBuffer[0];

            cbActual = 0;
            rcNt = NtQueryVirtualMemory(g_hProcess,
                                        (void const *)uPtrWhere,
                                        MemorySectionName,
                                        &uBuf,
                                        sizeof(uBuf),
                                        &cbActual);
            if (NT_SUCCESS(rcNt))
                RTTestIPrintf(RTTESTLVL_ALWAYS, "    %.*ls\n",
                              uBuf.Core.SectionFileName.Length / 2, uBuf.Core.SectionFileName.Buffer);
            else
            {
                RTTestIPrintf(RTTESTLVL_ALWAYS, "%p: MemorySectionName - rcNt=%#x\n", uPtrWhere, rcNt);
                RTTESTI_CHECK(rcNt == STATUS_FILE_INVALID && MemInfo.Type == MEM_MAPPED);
            }
        }

        /* Advance. */
        cbAdvance = MemInfo.RegionSize;
        //cbAdvance = 0;
        if (uPtrWhere + cbAdvance <= uPtrWhere)
            break;
        uPtrWhere += MemInfo.RegionSize;
    }
}


static void tstQueryInformationProcess(void)
{
    RTTestISub("NtQueryInformationProcess");

    NTSTATUS rcNt;

    /* Basic info */
    PROCESS_BASIC_INFORMATION BasicInfo;
    RT_ZERO(BasicInfo);
    DWORD cbActual = 0;
    rcNt = NtQueryInformationProcess(g_hProcess,
                                     ProcessBasicInformation,
                                     &BasicInfo, sizeof(BasicInfo), &cbActual);
    RTTESTI_CHECK_MSG(NT_SUCCESS(rcNt), ("rcNt=%#x\n", rcNt));
    if (NT_SUCCESS(rcNt))
        RTTestIPrintf(RTTESTLVL_ALWAYS, "BasicInfo:\n"
                                        "    UniqueProcessId = %#x (%6d)\n"
                                        "    PebBaseAddress  = %p\n"
                                        "    Reserved1       = %p          ExitStatus?\n"
                                        "    Reserved2a      = %p          AffinityMask?\n"
                                        "    Reserved2b      = %p (%6d) BasePriority?\n"
                                        "    Reserved3       = %p (%6d) InheritedFromUniqueProcessId?\n"
                      ,
                      BasicInfo.UniqueProcessId, BasicInfo.UniqueProcessId,
                      BasicInfo.PebBaseAddress,
                      BasicInfo.Reserved1,
                      BasicInfo.Reserved2[0],
                      BasicInfo.Reserved2[1], BasicInfo.Reserved2[1],
                      BasicInfo.Reserved3, BasicInfo.Reserved3
                      );


    /* Debugger present? */
    DWORD_PTR uPtr = ~(DWORD_PTR)0;
    cbActual = 0;
    rcNt = NtQueryInformationProcess(g_hProcess,
                                     ProcessDebugPort,
                                     &uPtr, sizeof(uPtr), &cbActual);
    RTTESTI_CHECK_MSG(NT_SUCCESS(rcNt), ("rcNt=%#x\n", rcNt));
    if (NT_SUCCESS(rcNt))
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessDebugPort:          %p\n", uPtr);

    /* Debug object handle, whatever that is... */
    uPtr = ~(DWORD_PTR)0;
    cbActual = 0;
    rcNt = NtQueryInformationProcess(g_hProcess,
                                     ProcessDebugObjectHandle,
                                     &uPtr, sizeof(uPtr), &cbActual);
    if (NT_SUCCESS(rcNt))
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessDebugObjectHandle:  %p\n", uPtr);
    else if (rcNt == STATUS_PORT_NOT_SET)
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessDebugObjectHandle:  rcNt=%#x (STATUS_PORT_NOT_SET)\n", uPtr);
    else
        RTTESTI_CHECK_MSG(NT_SUCCESS(rcNt), ("rcNt=%#x\n", rcNt));

    /* 32-bit app on 64-bit host? */
    uPtr = ~(DWORD_PTR)0;
    cbActual = 0;
    rcNt = NtQueryInformationProcess(g_hProcess,
                                     ProcessWow64Information,
                                     &uPtr, sizeof(uPtr), &cbActual);
    RTTESTI_CHECK_MSG(NT_SUCCESS(rcNt), ("rcNt=%#x\n", rcNt));
    if (NT_SUCCESS(rcNt))
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessWow64Information:   %p\n", uPtr);

    /* Process image name (NT). */
    struct
    {
        UNICODE_STRING     UniStr;
        WCHAR              awBuffer[UNICODE_STRING_MAX_CHARS];
    } StrBuf;
    RT_ZERO(StrBuf);
    StrBuf.UniStr.Length        = UNICODE_STRING_MAX_CHARS * 2;
    StrBuf.UniStr.MaximumLength = UNICODE_STRING_MAX_CHARS * 2;
    StrBuf.UniStr.Buffer        = &StrBuf.awBuffer[0];
    cbActual = 0;
    rcNt = NtQueryInformationProcess(g_hProcess,
                                     ProcessImageFileName,
                                     &StrBuf, sizeof(StrBuf), &cbActual);
    RTTESTI_CHECK_MSG(NT_SUCCESS(rcNt), ("rcNt=%#x\n", rcNt));
    if (NT_SUCCESS(rcNt))
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessImageFileName:      len=%u\n    %.*ls\n",
                      StrBuf.UniStr.Length, StrBuf.UniStr.Length, StrBuf.UniStr.Buffer);

    /* Process image name (Win32) - Not available on Windows 2003. */
    RT_ZERO(StrBuf);
    StrBuf.UniStr.Length        = UNICODE_STRING_MAX_CHARS * 2;
    StrBuf.UniStr.MaximumLength = UNICODE_STRING_MAX_CHARS * 2;
    StrBuf.UniStr.Buffer        = &StrBuf.awBuffer[0];
    cbActual = 0;
    rcNt = NtQueryInformationProcess(g_hProcess,
                                     ProcessImageFileNameWin32,
                                     &StrBuf, sizeof(StrBuf), &cbActual);
    if (rcNt != STATUS_INVALID_INFO_CLASS)
    {
        RTTESTI_CHECK_MSG(NT_SUCCESS(rcNt), ("rcNt=%#x\n", rcNt));
        if (NT_SUCCESS(rcNt))
            RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessImageFileNameWin32: len=%u\n    %.*ls\n",
                          StrBuf.UniStr.Length, StrBuf.UniStr.Length, StrBuf.UniStr.Buffer);
    }
    else
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessImageFileNameWin32: Not supported (STATUS_INVALID_INFO_CLASS).\n");

    /* Process image mapping - Not available on Windows 2003. */
    uPtr = ~(DWORD_PTR)0;
    cbActual = 0;
    rcNt = NtQueryInformationProcess(g_hProcess,
                                     ProcessImageFileMapping,
                                     &uPtr, sizeof(uPtr), &cbActual);
    if (NT_SUCCESS(rcNt))
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessImageFileMapping:   %p\n", uPtr);
    else if (rcNt == STATUS_OBJECT_TYPE_MISMATCH)
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessImageFileMapping:   rcNt=%#x (STATUS_OBJECT_TYPE_MISMATCH)\n", rcNt);
    else if (rcNt == STATUS_INVALID_INFO_CLASS)
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessImageFileMapping:   Not supported (STATUS_INVALID_INFO_CLASS).\n");
    else
        RTTestIFailed("ProcessImageFileMapping: rcNt=%#x\n", rcNt);


    /* Handles. Broken for 64-bit input. */
    uint32_t u32 = UINT32_MAX;
    cbActual = 0;
    rcNt = NtQueryInformationProcess(g_hProcess,
                                     ProcessHandleCount,
                                     &u32, sizeof(u32), &cbActual);
    if (NT_SUCCESS(rcNt))
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessHandleCount:        %#x (%d)\n", u32, u32);
    else
        RTTestIFailed("ProcessHandleCount: rcNt=%#x\n", rcNt);

    /* Execute flags. */
#if 0 /* fails... wrong process handle? */
    u32 = ~(DWORD_PTR)0;
    cbActual = 0;
    rcNt = NtQueryInformationProcess(g_hProcess,
                                     ProcessExecuteFlags,
                                     &u32, sizeof(u32), &cbActual);
    if (NT_SUCCESS(rcNt))
        RTTestIPrintf(RTTESTLVL_ALWAYS, "ProcessExecuteFlags:       %#p\n", u32);
    else
        RTTestIFailed("ProcessExecuteFlags: rcNt=%#x\n", rcNt);
#endif

    /** @todo ProcessImageInformation */
}


int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstNtQueryStuff", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    g_hProcess = GetCurrentProcess();

    //tstQueryVirtualMemory();
    tstQueryInformationProcess();


    return RTTestSummaryAndDestroy(g_hTest);
}



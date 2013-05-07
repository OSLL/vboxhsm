/* $Id: VBoxDispMpInternal.h 38114 2011-07-22 13:58:48Z vboxsync $ */

/** @file
 * VBoxVideo Display external interface config
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxDispMpInternal_h__
#define ___VBoxDispMpInternal_h__

#include <windows.h>

HRESULT vboxDispMpInternalInit();
HRESULT vboxDispMpInternalTerm();
HRESULT vboxDispMpInternalCancel(struct VBOXWDDMDISP_CONTEXT *pContext, struct VBOXWDDMDISP_SWAPCHAIN *pSwapchain);

#endif /* #ifndef ___VBoxDispMpInternal_h__ */
/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_environment.h"
#include "cr_string.h"
#include "cr_error.h"
#include "cr_mem.h"
#include "cr_spu.h"
#include "cr_environment.h"
#include "renderspu.h"
#include "cr_extstring.h"

#include <iprt/asm.h>


static void
DoSync(void)
{
    CRMessage *in, out;

    out.header.type = CR_MESSAGE_OOB;

    if (render_spu.is_swap_master)
    {
        int a;

        for (a = 0; a < render_spu.num_swap_clients; a++)
        {
            crNetGetMessage( render_spu.swap_conns[a], &in );
            crNetFree( render_spu.swap_conns[a], in);
        }

        for (a = 0; a < render_spu.num_swap_clients; a++)
            crNetSend( render_spu.swap_conns[a], NULL, &out, sizeof(CRMessage));
    }
    else
    {
        crNetSend( render_spu.swap_conns[0], NULL, &out, sizeof(CRMessage));

        crNetGetMessage( render_spu.swap_conns[0], &in );
        crNetFree( render_spu.swap_conns[0], in);
    }
}



/*
 * Visual functions
 */

/**
 * used for debugging and giving info to the user.
 */
void
renderspuMakeVisString( GLbitfield visAttribs, char *s )
{
    s[0] = 0;

    if (visAttribs & CR_RGB_BIT)
        crStrcat(s, "RGB");
    if (visAttribs & CR_ALPHA_BIT)
        crStrcat(s, "A");
    if (visAttribs & CR_DOUBLE_BIT)
        crStrcat(s, ", Doublebuffer");
    if (visAttribs & CR_STEREO_BIT)
        crStrcat(s, ", Stereo");
    if (visAttribs & CR_DEPTH_BIT)
        crStrcat(s, ", Z");
    if (visAttribs & CR_STENCIL_BIT)
        crStrcat(s, ", Stencil");
    if (visAttribs & CR_ACCUM_BIT)
        crStrcat(s, ", Accum");
    if (visAttribs & CR_MULTISAMPLE_BIT)
        crStrcat(s, ", Multisample");
    if (visAttribs & CR_OVERLAY_BIT)
        crStrcat(s, ", Overlay");
    if (visAttribs & CR_PBUFFER_BIT)
        crStrcat(s, ", PBuffer");
}

GLboolean renderspuInitVisual(VisualInfo *pVisInfo, const char *displayName, GLbitfield visAttribs)
{
    pVisInfo->displayName = crStrdup(displayName);
    pVisInfo->visAttribs = visAttribs;
    return renderspu_SystemInitVisual(pVisInfo);
}

/*
 * Find a VisualInfo which matches the given display name and attribute
 * bitmask, or return a pointer to a new visual.
 */
VisualInfo *
renderspuFindVisual(const char *displayName, GLbitfield visAttribs)
{
    int i;

    if (!displayName)
        displayName = "";

    /* first, try to find a match */
#if defined(WINDOWS) || defined(DARWIN)
    for (i = 0; i < render_spu.numVisuals; i++) {
        if (visAttribs == render_spu.visuals[i].visAttribs) {
            return &(render_spu.visuals[i]);
        }
    }
#elif defined(GLX)
    for (i = 0; i < render_spu.numVisuals; i++) {
        if (crStrcmp(displayName, render_spu.visuals[i].displayName) == 0
            && visAttribs == render_spu.visuals[i].visAttribs) {
            return &(render_spu.visuals[i]);
        }
    }
#endif

    if (render_spu.numVisuals >= MAX_VISUALS)
    {
        crWarning("Render SPU: Couldn't create a visual, too many visuals already");
        return NULL;
    }

    /* create a new visual */
    i = render_spu.numVisuals;
    if (renderspuInitVisual(&(render_spu.visuals[i]), displayName, visAttribs)) {
        render_spu.numVisuals++;
        return &(render_spu.visuals[i]);
    }
    else {
        crWarning("Render SPU: Couldn't get a visual, renderspu_SystemInitVisual failed");
        return NULL;
    }
}

static ContextInfo * renderspuCreateContextInternal(const char *dpyName, GLint visBits, GLint idCtx, ContextInfo * sharedContext)
{
    ContextInfo *context;
    VisualInfo *visual;

    if (idCtx <= 0)
    {
        idCtx = (GLint)crHashtableAllocKeys(render_spu.contextTable, 1);
        if (idCtx <= 0)
        {
            crWarning("failed to allocate context id");
            return NULL;
        }
    }
    else
    {
        if (crHashtableIsKeyUsed(render_spu.contextTable, idCtx))
        {
            crWarning("the specified ctx key %d is in use", idCtx);
            return NULL;
        }
    }


    if (!dpyName || crStrlen(render_spu.display_string)>0)
        dpyName = render_spu.display_string;

    visual = renderspuFindVisual(dpyName, visBits);
    if (!visual)
        return NULL;

    context = (ContextInfo *) crCalloc(sizeof(ContextInfo));
    if (!context)
        return NULL;
    context->BltInfo.Base.id = idCtx;
    context->shared = sharedContext;
    if (!renderspu_SystemCreateContext(visual, context, sharedContext))
        return NULL;

    crHashtableAdd(render_spu.contextTable, idCtx, context);

    context->BltInfo.Base.visualBits = visual->visAttribs;
    /*
    crDebug("Render SPU: CreateContext(%s, 0x%x) returning %d",
                    dpyName, visBits, context->BltInfo.Base.id);
    */

    if (sharedContext)
        ASMAtomicIncU32(&sharedContext->cRefs);
    context->cRefs = 1;

    return context;
}

GLint renderspuCreateContextEx(const char *dpyName, GLint visBits, GLint id, GLint shareCtx)
{
    ContextInfo *context, *sharedContext = NULL;

    if (shareCtx) {
        sharedContext
            = (ContextInfo *) crHashtableSearch(render_spu.contextTable, shareCtx);
        CRASSERT(sharedContext);
    }

    context = renderspuCreateContextInternal(dpyName, visBits, id, sharedContext);
    if (context)
        return context->BltInfo.Base.id;
    return -1;
}

/*
 * Context functions
 */

GLint RENDER_APIENTRY
renderspuCreateContext(const char *dpyName, GLint visBits, GLint shareCtx)
{
    return renderspuCreateContextEx(dpyName, visBits, 0, shareCtx);
}

static uint32_t renderspuContextRelease( ContextInfo *context );
static void renderspuDestroyContextTerminate( ContextInfo *context )
{
    CRASSERT(context->BltInfo.Base.id == -1);
    renderspu_SystemDestroyContext( context );
    if (context->extensionString) {
        crFree(context->extensionString);
        context->extensionString = NULL;
    }

    if (context->shared)
        renderspuContextRelease( context->shared );

    crFree(context);
}

static uint32_t renderspuContextRelease( ContextInfo *context )
{
    uint32_t cRefs = ASMAtomicDecU32(&context->cRefs);
    if (!cRefs)
        renderspuDestroyContextTerminate( context );
    else
        CRASSERT(cRefs < UINT32_MAX/2);
    return cRefs;
}

uint32_t renderspuContextMarkDeletedAndRelease( ContextInfo *context )
{
    /* invalidate the context id to mark it as deleted */
    context->BltInfo.Base.id = -1;

    /* some drivers do not like when the base (shared) context is deleted before its referals,
     * this is why we keep a context refference counting the base (shared) context will be destroyed as soon as*/
    return renderspuContextRelease( context );
}

static void RENDER_APIENTRY
renderspuDestroyContext( GLint ctx )
{
    ContextInfo *context, *curCtx;

    CRASSERT(ctx);

    if (ctx == CR_RENDER_DEFAULT_CONTEXT_ID)
    {
        crWarning("request to destroy a default context, ignoring");
        return;
    }

    context = (ContextInfo *) crHashtableSearch(render_spu.contextTable, ctx);
    CRASSERT(context);
    {
        if (!context)
        {
            crWarning("request to delete inexistent context");
            return;
        }
    }

    curCtx = GET_CONTEXT_VAL();
    CRASSERT(curCtx);
    if (curCtx == context)
    {
        renderspuMakeCurrent( CR_RENDER_DEFAULT_WINDOW_ID, 0, CR_RENDER_DEFAULT_CONTEXT_ID );
        curCtx = GET_CONTEXT_VAL();
		Assert(curCtx);
		Assert(curCtx != context);
    }

    crHashtableDelete(render_spu.contextTable, ctx, NULL);

    renderspuContextMarkDeletedAndRelease(context);
}


void RENDER_APIENTRY
renderspuMakeCurrent(GLint crWindow, GLint nativeWindow, GLint ctx)
{
    WindowInfo *window;
    ContextInfo *context;

    /*
    crDebug("%s win=%d native=0x%x ctx=%d", __FUNCTION__, crWindow, (int) nativeWindow, ctx);
    */

    window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, crWindow);
    context = (ContextInfo *) crHashtableSearch(render_spu.contextTable, ctx);

    if (window && context)
    {
#ifdef CHROMIUM_THREADSAFE
        crSetTSD(&_RenderTSD, context);
#else
        render_spu.currentContext = context;
#endif
        context->currentWindow = window;
        if (!window)
        {
            crDebug("Render SPU: MakeCurrent invalid window id: %d", crWindow);
            return;
        }
        if (!context)
        {
            crDebug("Render SPU: MakeCurrent invalid context id: %d", ctx);
            return;
        }

        renderspu_SystemMakeCurrent( window, nativeWindow, context );
        if (!context->everCurrent) {
            /* print OpenGL info */
            const char *extString = (const char *) render_spu.ws.glGetString( GL_EXTENSIONS );
            /*
            crDebug( "Render SPU: GL_EXTENSIONS:   %s", render_spu.ws.glGetString( GL_EXTENSIONS ) );
            */
            crInfo( "Render SPU: GL_VENDOR:   %s", render_spu.ws.glGetString( GL_VENDOR ) );
            crInfo( "Render SPU: GL_RENDERER: %s", render_spu.ws.glGetString( GL_RENDERER ) );
            crInfo( "Render SPU: GL_VERSION:  %s", render_spu.ws.glGetString( GL_VERSION ) );
            crInfo( "Render SPU: GL_EXTENSIONS: %s", render_spu.ws.glGetString( GL_EXTENSIONS ) );
            if (crStrstr(extString, "GL_ARB_window_pos"))
                context->haveWindowPosARB = GL_TRUE;
            else
                context->haveWindowPosARB = GL_FALSE;
            context->everCurrent = GL_TRUE;
        }
        if (crWindow == CR_RENDER_DEFAULT_WINDOW_ID && window->mapPending &&
                !render_spu.render_to_app_window && !render_spu.render_to_crut_window) {
            /* Window[CR_RENDER_DEFAULT_CONTEXT_ID] is special, it's the default window and normally hidden.
             * If the mapPending flag is set, then we should now make the window
             * visible.
             */
            /*renderspu_SystemShowWindow( window, GL_TRUE );*/
            window->mapPending = GL_FALSE;
        }
        window->everCurrent = GL_TRUE;
    }
    else if (!crWindow && !ctx)
    {
        renderspu_SystemMakeCurrent( NULL, 0, NULL );
#ifdef CHROMIUM_THREADSAFE
        crSetTSD(&_RenderTSD, NULL);
#else
        render_spu.currentContext = NULL;
#endif
    }
    else
    {
        crError("renderspuMakeCurrent invalid ids: crWindow(%d), ctx(%d)", crWindow, ctx);
    }
}

GLboolean renderspuWindowInit( WindowInfo *window, VisualInfo *visual, GLboolean showIt, GLint id )
{
    crMemset(window, 0, sizeof (*window));
    RTCritSectInit(&window->CompositorLock);
    window->fCompositorPresentEmpty = GL_FALSE;
    window->pCompositor = NULL;

    window->BltInfo.Base.id = id;

    window->x = render_spu.defaultX;
    window->y = render_spu.defaultY;
    window->BltInfo.width  = render_spu.defaultWidth;
    window->BltInfo.height = render_spu.defaultHeight;

    /* Set window->title, replacing %i with the window ID number */
    {
        const char *s = crStrstr(render_spu.window_title, "%i");
        if (s) {
            int i, j, k;
            window->title = crAlloc(crStrlen(render_spu.window_title) + 10);
            for (i = 0; render_spu.window_title[i] != '%'; i++)
                window->title[i] = render_spu.window_title[i];
            k = sprintf(window->title + i, "%d", window->BltInfo.Base.id);
            CRASSERT(k < 10);
            i++; /* skip the 'i' after the '%' */
            j = i + k;
            for (; (window->title[j] = s[i]) != 0; i++, j++)
                ;
        }
        else {
            window->title = crStrdup(render_spu.window_title);
        }
    }
        
    window->BltInfo.Base.visualBits = visual->visAttribs;
    
    
    /*
    crDebug("Render SPU: Creating window (visBits=0x%x, id=%d)", visBits, window->BltInfo.Base.id);
    */
    /* Have GLX/WGL/AGL create the window */
    if (!renderspu_SystemVBoxCreateWindow( visual, showIt, window ))
    {
        crWarning( "Render SPU: Couldn't create a window, renderspu_SystemCreateWindow failed" );
        return GL_FALSE;
    }
    
    window->visible = !!showIt;

    CRASSERT(window->visual == visual);
    return GL_TRUE;
}

/*
 * Window functions
 */

GLint renderspuWindowCreateEx( const char *dpyName, GLint visBits, GLint id )
{
    WindowInfo *window;
    VisualInfo *visual;
    GLboolean showIt;

    if (id <= 0)
    {
        id = (GLint)crHashtableAllocKeys(render_spu.windowTable, 1);
        if (id <= 0)
        {
            crWarning("failed to allocate window id");
            return -1;
        }
    }
    else
    {
        if (crHashtableIsKeyUsed(render_spu.windowTable, id))
        {
            crWarning("the specified window key %d is in use", id);
            return -1;
        }
    }


    if (!dpyName || crStrlen(render_spu.display_string) > 0)
        dpyName = render_spu.display_string;

    visual = renderspuFindVisual( dpyName, visBits );
    if (!visual)
    {
        crWarning( "Render SPU: Couldn't create a window, renderspuFindVisual returned NULL" );
        return -1;
    }

    /* Allocate WindowInfo */
    window = (WindowInfo *) crCalloc(sizeof(WindowInfo));
    if (!window)
    {
        crWarning( "Render SPU: Couldn't create a window" );
        return -1;
    }
    
    crHashtableAdd(render_spu.windowTable, id, window);

    showIt = 0;

    /*
    crDebug("Render SPU: Creating window (visBits=0x%x, id=%d)", visBits, window->BltInfo.Base.id);
    */
    /* Have GLX/WGL/AGL create the window */
    if (!renderspuWindowInit( window, visual, showIt, id ))
    {
        crFree(window);
        crWarning( "Render SPU: Couldn't create a window, renderspu_SystemCreateWindow failed" );
        return -1;
    }
    
    return window->BltInfo.Base.id;
}

GLint RENDER_APIENTRY
renderspuWindowCreate( const char *dpyName, GLint visBits )
{
    return renderspuWindowCreateEx( dpyName, visBits, 0 );
}

static void renderspuCheckCurrentCtxWindowCB(unsigned long key, void *data1, void *data2)
{
    ContextInfo *pCtx = (ContextInfo *) data1;
    WindowInfo *pWindow = data2;
    (void) key;

    if (pCtx->currentWindow==pWindow)
    {
        renderspuMakeCurrent(CR_RENDER_DEFAULT_WINDOW_ID, 0, pCtx->BltInfo.Base.id);
        pCtx->currentWindow=0;
    }
}

void renderspuWindowTerm( WindowInfo *window )
{
    GET_CONTEXT(pOldCtx);
    /* ensure no concurrent draws can take place */
    renderspuVBoxCompositorSet(window, NULL);
    renderspuVBoxPresentBlitterCleanup(window);
    renderspu_SystemDestroyWindow( window );
    RTCritSectDelete(&window->CompositorLock);
    /* check if this window is bound to some ctx. Note: window pointer is already freed here */
    crHashtableWalk(render_spu.contextTable, renderspuCheckCurrentCtxWindowCB, window);
    /* restore current context */
    {
        GET_CONTEXT(pNewCtx);
        if (pNewCtx!=pOldCtx)
        {
            renderspuMakeCurrent(pOldCtx&&pOldCtx->currentWindow ? pOldCtx->currentWindow->BltInfo.Base.id:CR_RENDER_DEFAULT_WINDOW_ID, 0,
                                     pOldCtx ? pOldCtx->BltInfo.Base.id:CR_RENDER_DEFAULT_CONTEXT_ID);
        }
    }
}

void
RENDER_APIENTRY renderspuWindowDestroy( GLint win )
{
    WindowInfo *window;

    CRASSERT(win >= 0);
    if (win == CR_RENDER_DEFAULT_WINDOW_ID)
    {
        crWarning("request to destroy a default mural, ignoring");
        return;
    }
    window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, win);
    if (window) {
        crDebug("Render SPU: Destroy window (%d)", win);
        renderspuWindowTerm( window );

        /* remove window info from hash table, and free it */
        crHashtableDelete(render_spu.windowTable, win, crFree);

    }
    else {
        crDebug("Render SPU: Attempt to destroy invalid window (%d)", win);
    }
}


static void RENDER_APIENTRY
renderspuWindowSize( GLint win, GLint w, GLint h )
{
    WindowInfo *window;
    CRASSERT(win >= 0);
    window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, win);
    if (window) {
        if (w != window->BltInfo.width
                || h != window->BltInfo.height)
        {
            /* window is resized, compositor data is no longer valid
             * this set also ensures all redraw operations are done in the redraw thread
             * and that no redraw is started until new Present request comes containing a valid presentation data */
            renderspuVBoxCompositorSet( window, NULL);
            renderspu_SystemWindowSize( window, w, h );
            window->BltInfo.width  = w;
            window->BltInfo.height = h;
        }
    }
    else {
        crDebug("Render SPU: Attempt to resize invalid window (%d)", win);
    }
}


static void RENDER_APIENTRY
renderspuWindowPosition( GLint win, GLint x, GLint y )
{
    if (!render_spu.ignore_window_moves) {
        WindowInfo *window;
        CRASSERT(win >= 0);
        window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, win);
        if (window) {
            renderspu_SystemWindowPosition( window, x, y );
            window->x = x;
            window->y = y;
        }
        else {
            crDebug("Render SPU: Attempt to move invalid window (%d)", win);
        }
    }
}

static void RENDER_APIENTRY
renderspuWindowVisibleRegion(GLint win, GLint cRects, const GLint *pRects)
{
    WindowInfo *window;
    CRASSERT(win >= 0);
    window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, win);
    if (window) {
        renderspu_SystemWindowVisibleRegion( window, cRects, pRects );
    }
    else {
        crDebug("Render SPU: Attempt to set VisibleRegion for invalid window (%d)", win);
    }
}

static void RENDER_APIENTRY
renderspuWindowShow( GLint win, GLint flag )
{
    WindowInfo *window;
    CRASSERT(win >= 0);
    window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, win);
    if (window) {
        GLboolean visible;
        if (window->nativeWindow) {
            /* We're rendering back to the native app window instead of the
             * new window which we (the Render SPU) created earlier.
             * So, we never want to show the Render SPU's window.
             */
            flag = 0;
        }
        
        visible = !!flag;
        
        if (window->visible != visible)
        {
            renderspu_SystemShowWindow( window, visible );
            window->visible = visible;
        }
    }
    else {
        crDebug("Render SPU: Attempt to hide/show invalid window (%d)", win);
    }
}

static void RENDER_APIENTRY
renderspuVBoxPresentComposition( GLint win, struct VBOXVR_SCR_COMPOSITOR * pCompositor, struct VBOXVR_SCR_COMPOSITOR_ENTRY *pChangedEntry )
{
    WindowInfo *window;
    CRASSERT(win >= 0);
    window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, win);
    if (window) {
        if (pCompositor && CrVrScrCompositorIsEmpty(pCompositor) && !window->fCompositorPresentEmpty)
            pCompositor = NULL;

        if (pCompositor)
            window->fCompositorPresentEmpty = GL_FALSE;

        renderspuVBoxCompositorSet( window, pCompositor);
        if (pCompositor)
        {
            renderspu_SystemVBoxPresentComposition(window, pChangedEntry);
        }
    }
    else {
        crDebug("Render SPU: Attempt to PresentComposition for invalid window (%d)", win);
    }
}

void renderspuVBoxCompositorBlitStretched ( struct VBOXVR_SCR_COMPOSITOR * pCompositor, PCR_BLITTER pBlitter, GLfloat scaleX, GLfloat scaleY)
{
    VBOXVR_SCR_COMPOSITOR_ITERATOR CIter;
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry;
    CrVrScrCompositorIterInit(pCompositor, &CIter);
    while ((pEntry = CrVrScrCompositorIterNext(&CIter)) != NULL)
    {
        uint32_t cRegions;
        const RTRECT *paSrcRegions, *paDstRegions;
        int rc = CrVrScrCompositorEntryRegionsGet(pCompositor, pEntry, &cRegions, &paSrcRegions, &paDstRegions);
        if (RT_SUCCESS(rc))
        {
            uint32_t i;
            for (i = 0; i < cRegions; ++i)
            {
                RTRECT DstRect;
                DstRect.xLeft = paDstRegions[i].xLeft * scaleX;
                DstRect.yTop = paDstRegions[i].yTop * scaleY;
                DstRect.xRight = paDstRegions[i].xRight * scaleX;
                DstRect.yBottom = paDstRegions[i].yBottom * scaleY;
                CrBltBlitTexMural(pBlitter, &pEntry->Tex, &paSrcRegions[i], &DstRect, 1, CRBLT_F_LINEAR | CRBLT_F_INVERT_YCOORDS);
            }
        }
        else
        {
            crWarning("BlitStretched: CrVrScrCompositorEntryRegionsGet failed rc %d", rc);
        }
    }
}

void renderspuVBoxCompositorBlit ( struct VBOXVR_SCR_COMPOSITOR * pCompositor, PCR_BLITTER pBlitter)
{
    VBOXVR_SCR_COMPOSITOR_ITERATOR CIter;
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry;
    CrVrScrCompositorIterInit(pCompositor, &CIter);
    while ((pEntry = CrVrScrCompositorIterNext(&CIter)) != NULL)
    {
        uint32_t cRegions;
        const RTRECT *paSrcRegions, *paDstRegions;
        int rc = CrVrScrCompositorEntryRegionsGet(pCompositor, pEntry, &cRegions, &paSrcRegions, &paDstRegions);
        if (RT_SUCCESS(rc))
        {
            CrBltBlitTexMural(pBlitter, &pEntry->Tex, paSrcRegions, paDstRegions, cRegions, CRBLT_F_LINEAR | CRBLT_F_INVERT_YCOORDS);
        }
        else
        {
            crWarning("Blit: CrVrScrCompositorEntryRegionsGet failed rc %d", rc);
        }
    }
}

void renderspuVBoxPresentBlitterCleanup( WindowInfo *window )
{
    if (!window->pBlitter)
        return;

    if (render_spu.blitterTable)
    {
        const CR_BLITTER_WINDOW * pBltInfo = CrBltMuralGetCurrentInfo(window->pBlitter);
        if (pBltInfo->Base.id == window->BltInfo.Base.id)
        {
            CrBltMuralSetCurrent(window->pBlitter, NULL);
        }
    }
    else
    {
        CRASSERT(CrBltMuralGetCurrentInfo(window->pBlitter)->Base.id == window->BltInfo.Base.id);
        CrBltMuralSetCurrent(window->pBlitter, NULL);
        CrBltTerm(window->pBlitter);
    }
    window->pBlitter = NULL;
}

PCR_BLITTER renderspuVBoxPresentBlitterGet( WindowInfo *window )
{
    PCR_BLITTER pBlitter = window->pBlitter;
    if (!pBlitter)
    {
        if (render_spu.blitterTable)
        {
            crHashtableLock(render_spu.blitterTable);
            pBlitter = (PCR_BLITTER)crHashtableSearch(render_spu.blitterTable, window->visual->visAttribs);
        }

        if (!pBlitter)
        {
            int rc;
            CR_BLITTER_CONTEXT ctx;
            pBlitter = (PCR_BLITTER)crCalloc(sizeof (*pBlitter));
            if (!pBlitter)
            {
                crWarning("failed to allocate blitter");
                return NULL;
            }

            /* @todo: this is the assumption that crserverlib uses context 1 as a default one
             * need to do it in a more proper way */
            ctx.Base.id = 1;
            ctx.Base.visualBits = window->visual->visAttribs;
            rc = CrBltInit(pBlitter, &ctx, true, true, render_spu.blitterDispatch);
            if (!RT_SUCCESS(rc))
            {
                crWarning("CrBltInit failed, rc %d", rc);
                crFree(pBlitter);
                return NULL;
            }

            if (render_spu.blitterTable)
            {
                crHashtableAdd( render_spu.blitterTable, window->visual->visAttribs, pBlitter );
            }
        }

        if (render_spu.blitterTable)
            crHashtableUnlock(render_spu.blitterTable);

        Assert(pBlitter);
        window->pBlitter = pBlitter;
    }

    CrBltMuralSetCurrent(pBlitter, &window->BltInfo);
    return pBlitter;
}

int renderspuVBoxPresentBlitterEnter( PCR_BLITTER pBlitter, int32_t i32MakeCurrentUserData)
{
    int rc;
    PCR_BLITTER_CONTEXT pCtxInfo = NULL;
    PCR_BLITTER_WINDOW pWindowInfo = NULL;
    GET_CONTEXT(pCtx);

    if (pCtx)
    {
        if (pCtx->currentWindow)
        {
            pCtxInfo = &pCtx->BltInfo;
            pWindowInfo =  &pCtx->currentWindow->BltInfo;
        }
    }

    CrBltSetMakeCurrentUserData(pBlitter, i32MakeCurrentUserData);

    rc = CrBltEnter(pBlitter, pCtxInfo, pWindowInfo);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrBltEnter failed, rc %d", rc);
        return rc;
    }
    return VINF_SUCCESS;
}

PCR_BLITTER renderspuVBoxPresentBlitterGetAndEnter( WindowInfo *window, int32_t i32MakeCurrentUserData )
{
    PCR_BLITTER pBlitter = renderspuVBoxPresentBlitterGet(window);
    if (pBlitter)
    {
        int rc = renderspuVBoxPresentBlitterEnter(pBlitter, i32MakeCurrentUserData);
        if (RT_SUCCESS(rc))
        {
            return pBlitter;
        }
    }
    return NULL;
}

PCR_BLITTER renderspuVBoxPresentBlitterEnsureCreated( WindowInfo *window, int32_t i32MakeCurrentUserData )
{
    if (!window->pBlitter)
    {
        struct VBOXVR_SCR_COMPOSITOR * pTmpCompositor;
        /* just use compositor lock to synchronize */
        pTmpCompositor = renderspuVBoxCompositorAcquire(window);
        CRASSERT(pTmpCompositor);
        if (pTmpCompositor)
        {
            PCR_BLITTER pBlitter = renderspuVBoxPresentBlitterGet( window );
            if (pBlitter)
            {
                if (!CrBltIsEverEntered(pBlitter))
                {
                    int rc = renderspuVBoxPresentBlitterEnter(pBlitter, i32MakeCurrentUserData);
                    if (RT_SUCCESS(rc))
                    {
                        CrBltLeave(pBlitter);
                    }
                    else
                    {
                        crWarning("renderspuVBoxPresentBlitterEnter failed rc %d", rc);
                    }
                }
            }
            else
            {
                crWarning("renderspuVBoxPresentBlitterGet failed");
            }

            renderspuVBoxCompositorRelease(window);
        }
        else
        {
            crWarning("renderspuVBoxCompositorAcquire failed");
        }
    }
    return window->pBlitter;
}

void renderspuVBoxPresentCompositionGeneric( WindowInfo *window, struct VBOXVR_SCR_COMPOSITOR * pCompositor, struct VBOXVR_SCR_COMPOSITOR_ENTRY *pChangedEntry, int32_t i32MakeCurrentUserData )
{
    PCR_BLITTER pBlitter = renderspuVBoxPresentBlitterGetAndEnter(window, i32MakeCurrentUserData);
    if (!pBlitter)
        return;

    renderspuVBoxCompositorBlit(pCompositor, pBlitter);

    renderspu_SystemSwapBuffers(window, 0);

    CrBltLeave(pBlitter);
}

void renderspuVBoxCompositorSet( WindowInfo *window, struct VBOXVR_SCR_COMPOSITOR * pCompositor)
{
    int rc;
    /* renderspuVBoxCompositorSet can be invoked from the chromium thread only and is not reentrant,
     * no need to synch here
     * the lock is actually needed to ensure we're in synch with the redraw thread */
    if (window->pCompositor == pCompositor)
        return;
    rc = RTCritSectEnter(&window->CompositorLock);
    if (RT_SUCCESS(rc))
    {
        window->pCompositor = pCompositor;
        RTCritSectLeave(&window->CompositorLock);
        return;
    }
    else
    {
        crWarning("RTCritSectEnter failed rc %d", rc);
    }
}

static void renderspuVBoxCompositorClearAllCB(unsigned long key, void *data1, void *data2)
{
    WindowInfo *window = (WindowInfo *) data1;
    renderspuVBoxCompositorSet(window, NULL);
}

void renderspuVBoxCompositorClearAll()
{
    /* we need to clear window compositor, which is not that trivial though,
     * since the lock order used in presentation thread is compositor lock() -> hash table lock (aquired for id->window resolution)
     * this is why, to prevent potential deadlocks, we use crHashtableWalkUnlocked that does not hold the table lock
     * we are can be sure noone will modify the table here since renderspuVBoxCompositorClearAll can be called in the command (hgcm) thread only,
     * and the table can be modified from that thread only as well */
    crHashtableWalkUnlocked(render_spu.windowTable, renderspuVBoxCompositorClearAllCB, NULL);
}

struct VBOXVR_SCR_COMPOSITOR * renderspuVBoxCompositorAcquire( WindowInfo *window)
{
    int rc = RTCritSectEnter(&window->CompositorLock);
    if (RT_SUCCESS(rc))
    {
        VBOXVR_SCR_COMPOSITOR * pCompositor = window->pCompositor;
        if (pCompositor)
            return pCompositor;

        /* if no compositor is set, release the lock and return */
        RTCritSectLeave(&window->CompositorLock);
    }
    else
    {
        crWarning("RTCritSectEnter failed rc %d", rc);
    }
    return NULL;
}

int renderspuVBoxCompositorLock(WindowInfo *window)
{
    int rc = RTCritSectEnter(&window->CompositorLock);
    AssertRC(rc);
    return rc;
}

int renderspuVBoxCompositorUnlock(WindowInfo *window)
{
    int rc = RTCritSectLeave(&window->CompositorLock);
    AssertRC(rc);
    return rc;
}

int renderspuVBoxCompositorTryAcquire(WindowInfo *window, struct VBOXVR_SCR_COMPOSITOR **ppCompositor)
{
    int rc = RTCritSectTryEnter(&window->CompositorLock);
    if (RT_SUCCESS(rc))
    {
        *ppCompositor = window->pCompositor;
        if (*ppCompositor)
            return VINF_SUCCESS;

        /* if no compositor is set, release the lock and return */
        RTCritSectLeave(&window->CompositorLock);
        rc = VERR_INVALID_STATE;
    }
    else
    {
        *ppCompositor = NULL;
    }
    return rc;
}

void renderspuVBoxCompositorRelease( WindowInfo *window)
{
    int rc;
    Assert(window->pCompositor);
    if (CrVrScrCompositorIsEmpty(window->pCompositor) && RTCritSectGetRecursion(&window->CompositorLock) == 1)
        window->pCompositor = NULL;
    rc = RTCritSectLeave(&window->CompositorLock);
    if (!RT_SUCCESS(rc))
    {
        crWarning("RTCritSectLeave failed rc %d", rc);
    }
}


/*
 * Set the current raster position to the given window coordinate.
 */
static void
SetRasterPos( GLint winX, GLint winY )
{
    GLfloat fx, fy;

    /* Push current matrix mode and viewport attributes */
    render_spu.self.PushAttrib( GL_TRANSFORM_BIT | GL_VIEWPORT_BIT );

    /* Setup projection parameters */
    render_spu.self.MatrixMode( GL_PROJECTION );
    render_spu.self.PushMatrix();
    render_spu.self.LoadIdentity();
    render_spu.self.MatrixMode( GL_MODELVIEW );
    render_spu.self.PushMatrix();
    render_spu.self.LoadIdentity();

    render_spu.self.Viewport( winX - 1, winY - 1, 2, 2 );

    /* set the raster (window) position */
    /* huh ? */
    fx = (GLfloat) (winX - (int) winX);
    fy = (GLfloat) (winY - (int) winY);
    render_spu.self.RasterPos4f( fx, fy, 0.0, 1.0 );

    /* restore matrices, viewport and matrix mode */
    render_spu.self.PopMatrix();
    render_spu.self.MatrixMode( GL_PROJECTION );
    render_spu.self.PopMatrix();

    render_spu.self.PopAttrib();
}


/*
 * Draw the mouse pointer bitmap at (x,y) in window coords.
 */
static void DrawCursor( GLint x, GLint y )
{
#define POINTER_WIDTH   32
#define POINTER_HEIGHT  32
    /* Somebody artistic could probably do better here */
    static const char *pointerImage[POINTER_HEIGHT] =
    {
        "XX..............................",
        "XXXX............................",
        ".XXXXX..........................",
        ".XXXXXXX........................",
        "..XXXXXXXX......................",
        "..XXXXXXXXXX....................",
        "...XXXXXXXXXXX..................",
        "...XXXXXXXXXXXXX................",
        "....XXXXXXXXXXXXXX..............",
        "....XXXXXXXXXXXXXXXX............",
        ".....XXXXXXXXXXXXXXXXX..........",
        ".....XXXXXXXXXXXXXXXXXXX........",
        "......XXXXXXXXXXXXXXXXXXXX......",
        "......XXXXXXXXXXXXXXXXXXXXXX....",
        ".......XXXXXXXXXXXXXXXXXXXXXXX..",
        ".......XXXXXXXXXXXXXXXXXXXXXXXX.",
        "........XXXXXXXXXXXXX...........",
        "........XXXXXXXX.XXXXX..........",
        ".........XXXXXX...XXXXX.........",
        ".........XXXXX.....XXXXX........",
        "..........XXX.......XXXXX.......",
        "..........XX.........XXXXX......",
        "......................XXXXX.....",
        ".......................XXXXX....",
        "........................XXX.....",
        ".........................X......",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................"

    };
    static GLubyte pointerBitmap[POINTER_HEIGHT][POINTER_WIDTH / 8];
    static GLboolean firstCall = GL_TRUE;
    GLboolean lighting, depthTest, scissorTest;

    if (firstCall) {
        /* Convert pointerImage into pointerBitmap */
        GLint i, j;
        for (i = 0; i < POINTER_HEIGHT; i++) {
            for (j = 0; j < POINTER_WIDTH; j++) {
                if (pointerImage[POINTER_HEIGHT - i - 1][j] == 'X') {
                    GLubyte bit = 128 >> (j & 0x7);
                    pointerBitmap[i][j / 8] |= bit;
                }
            }
        }
        firstCall = GL_FALSE;
    }

    render_spu.self.GetBooleanv(GL_LIGHTING, &lighting);
    render_spu.self.GetBooleanv(GL_DEPTH_TEST, &depthTest);
    render_spu.self.GetBooleanv(GL_SCISSOR_TEST, &scissorTest);
    render_spu.self.Disable(GL_LIGHTING);
    render_spu.self.Disable(GL_DEPTH_TEST);
    render_spu.self.Disable(GL_SCISSOR_TEST);
    render_spu.self.PixelStorei(GL_UNPACK_ALIGNMENT, 1);

    render_spu.self.Color3f(1, 1, 1);

    /* save current raster pos */
    render_spu.self.PushAttrib(GL_CURRENT_BIT);
    SetRasterPos(x, y);
    render_spu.self.Bitmap(POINTER_WIDTH, POINTER_HEIGHT, 1.0, 31.0, 0, 0,
                                (const GLubyte *) pointerBitmap);
    /* restore current raster pos */
    render_spu.self.PopAttrib();

    if (lighting)
       render_spu.self.Enable(GL_LIGHTING);
    if (depthTest)
       render_spu.self.Enable(GL_DEPTH_TEST);
    if (scissorTest)
        render_spu.self.Enable(GL_SCISSOR_TEST);
}

void RENDER_APIENTRY renderspuSwapBuffers( GLint window, GLint flags )
{
    WindowInfo *w = (WindowInfo *) crHashtableSearch(render_spu.windowTable, window);

    if (!w)
    {
        crDebug("Render SPU: SwapBuffers invalid window id: %d", window);
        return;
    }

    if (flags & CR_SUPPRESS_SWAP_BIT)
    {
        render_spu.self.Finish();
        return;
    }

    if (render_spu.drawCursor)
        DrawCursor( render_spu.cursorX, render_spu.cursorY );

    if (render_spu.swap_master_url)
        DoSync();

    renderspu_SystemSwapBuffers( w, flags );
}


/*
 * Barrier functions
 * Normally, we'll have a crserver somewhere that handles the barrier calls.
 * However, if we're running the render SPU on the client node, then we
 * should handle barriers here.  The threadtest demo illustrates this.
 * If we have N threads calling using this SPU we need these barrier
 * functions to synchronize them.
 */

static void RENDER_APIENTRY renderspuBarrierCreateCR( GLuint name, GLuint count )
{
    Barrier *b;

    if (render_spu.ignore_papi)
        return;

    b = (Barrier *) crHashtableSearch( render_spu.barrierHash, name );
    if (b) {
        /* HACK -- this allows everybody to create a barrier, and all
           but the first creation are ignored, assuming the count
           match. */
        if ( b->count != count ) {
            crError( "Render SPU: Barrier name=%u created with count=%u, but already "
                     "exists with count=%u", name, count, b->count );
        }
    }
    else {
        b = (Barrier *) crAlloc( sizeof(Barrier) );
        b->count = count;
        crInitBarrier( &b->barrier, count );
        crHashtableAdd( render_spu.barrierHash, name, b );
    }
}

static void RENDER_APIENTRY renderspuBarrierDestroyCR( GLuint name )
{
    if (render_spu.ignore_papi)
        return;
    crHashtableDelete( render_spu.barrierHash, name, crFree );
}

static void RENDER_APIENTRY renderspuBarrierExecCR( GLuint name )
{
    Barrier *b;

    if (render_spu.ignore_papi)
        return;

    b = (Barrier *) crHashtableSearch( render_spu.barrierHash, name );
    if (b) {
        crWaitBarrier( &(b->barrier) );
    }
    else {
        crWarning("Render SPU: Bad barrier name %d in BarrierExec()", name);
    }
}


/*
 * Semaphore functions
 * XXX we should probably implement these too, for the same reason as
 * barriers (see above).
 */

static void RENDER_APIENTRY renderspuSemaphoreCreateCR( GLuint name, GLuint count )
{
    (void) name;
    (void) count;
}

static void RENDER_APIENTRY renderspuSemaphoreDestroyCR( GLuint name )
{
    (void) name;
}

static void RENDER_APIENTRY renderspuSemaphorePCR( GLuint name )
{
    (void) name;
}

static void RENDER_APIENTRY renderspuSemaphoreVCR( GLuint name )
{
    (void) name;
}


/*
 * Misc functions
 */



static void RENDER_APIENTRY renderspuChromiumParameteriCR(GLenum target, GLint value)
{

    switch (target)
    {
        default:
//            crWarning("Unhandled target in renderspuChromiumParameteriCR()");
            break;
    }
}

static void RENDER_APIENTRY
renderspuChromiumParameterfCR(GLenum target, GLfloat value)
{
    (void) target;
    (void) value;

#if 0
    switch (target) {
    default:
        crWarning("Unhandled target in renderspuChromiumParameterfCR()");
        break;
    }
#endif
}


static void RENDER_APIENTRY
renderspuChromiumParametervCR(GLenum target, GLenum type, GLsizei count,
                                                            const GLvoid *values)
{
    int client_num;
    unsigned short port;
    CRMessage *msg, pingback;
    unsigned char *privbuf = NULL;

    switch (target) {

        case GL_GATHER_CONNECT_CR:
            if (render_spu.gather_userbuf_size)
                privbuf = (unsigned char *)crAlloc(1024*768*4);

            port = ((GLint *) values)[0];

            if (render_spu.gather_conns == NULL)
                render_spu.gather_conns = crAlloc(render_spu.server->numClients*sizeof(CRConnection *));
            else
            {
                crError("Oh bother! duplicate GL_GATHER_CONNECT_CR getting through");
            }

            for (client_num=0; client_num< render_spu.server->numClients; client_num++)
            {
                switch (render_spu.server->clients[client_num]->conn->type)
                {
                    case CR_TCPIP:
                        crDebug("Render SPU: AcceptClient from %s on %d",
                            render_spu.server->clients[client_num]->conn->hostname, render_spu.gather_port);
                        render_spu.gather_conns[client_num] =
                                crNetAcceptClient("tcpip", NULL, port, 1024*1024,  1);
                        break;

                    case CR_GM:
                        render_spu.gather_conns[client_num] =
                                crNetAcceptClient("gm", NULL, port, 1024*1024,  1);
                        break;

                    default:
                        crError("Render SPU: Unknown Network Type to Open Gather Connection");
                }


                if (render_spu.gather_userbuf_size)
                {
                    render_spu.gather_conns[client_num]->userbuf = privbuf;
                    render_spu.gather_conns[client_num]->userbuf_len = render_spu.gather_userbuf_size;
                }
                else
                {
                    render_spu.gather_conns[client_num]->userbuf = NULL;
                    render_spu.gather_conns[client_num]->userbuf_len = 0;
                }

                if (render_spu.gather_conns[client_num])
                {
                    crDebug("Render SPU: success! from %s", render_spu.gather_conns[client_num]->hostname);
                }
            }

            break;

    case GL_GATHER_DRAWPIXELS_CR:
        pingback.header.type = CR_MESSAGE_OOB;

        for (client_num=0; client_num< render_spu.server->numClients; client_num++)
        {
            crNetGetMessage(render_spu.gather_conns[client_num], &msg);
            if (msg->header.type == CR_MESSAGE_GATHER)
            {
                crNetFree(render_spu.gather_conns[client_num], msg);
            }
            else
            {
                crError("Render SPU: expecting MESSAGE_GATHER. got crap! (%d of %d)",
                                client_num, render_spu.server->numClients-1);
            }
        }

        /*
         * We're only hitting the case if we're not actually calling
         * child.SwapBuffers from readback, so a switch about which
         * call to DoSync() we really want [this one, or the one
         * in SwapBuffers above] is not necessary -- karl
         */

        if (render_spu.swap_master_url)
            DoSync();

        for (client_num=0; client_num< render_spu.server->numClients; client_num++)
            crNetSend(render_spu.gather_conns[client_num], NULL, &pingback,
                                        sizeof(CRMessageHeader));

        render_spu.self.RasterPos2i(((GLint *)values)[0], ((GLint *)values)[1]);
        render_spu.self.DrawPixels(  ((GLint *)values)[2], ((GLint *)values)[3],
                                        ((GLint *)values)[4], ((GLint *)values)[5],
                                    render_spu.gather_conns[0]->userbuf);


        render_spu.self.SwapBuffers(((GLint *)values)[6], 0);
        break;

    case GL_CURSOR_POSITION_CR:
        if (type == GL_INT && count == 2) {
            render_spu.cursorX = ((GLint *) values)[0];
            render_spu.cursorY = ((GLint *) values)[1];
            crDebug("Render SPU: GL_CURSOR_POSITION_CR (%d, %d)", render_spu.cursorX, render_spu.cursorY);
        }
        else {
            crWarning("Render SPU: Bad type or count for ChromiumParametervCR(GL_CURSOR_POSITION_CR)");
        }
        break;

    case GL_WINDOW_SIZE_CR:
        /* XXX this is old code that should be removed.
         * NOTE: we can only resize the default (id=CR_RENDER_DEFAULT_WINDOW_ID) window!!!
         */
        {
            GLint w, h;
            WindowInfo *window;
            CRASSERT(type == GL_INT);
            CRASSERT(count == 2);
            CRASSERT(values);
            w = ((GLint*)values)[0];
            h = ((GLint*)values)[1];
            window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, CR_RENDER_DEFAULT_WINDOW_ID);
            if (window)
            {
                renderspu_SystemWindowSize(window, w, h);
            }
        }
        break;

    default:
#if 0
        crWarning("Unhandled target in renderspuChromiumParametervCR(0x%x)", (int) target);
#endif
        break;
    }
}


static void RENDER_APIENTRY
renderspuGetChromiumParametervCR(GLenum target, GLuint index, GLenum type,
                                                                 GLsizei count, GLvoid *values)
{
    switch (target) {
    case GL_WINDOW_SIZE_CR:
        {
            GLint x, y, w, h, *size = (GLint *) values;
            WindowInfo *window;
            CRASSERT(type == GL_INT);
            CRASSERT(count == 2);
            CRASSERT(values);
            size[0] = size[1] = 0;  /* default */
            window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, index);
            if (window)
            {
                renderspu_SystemGetWindowGeometry(window, &x, &y, &w, &h);
                size[0] = w;
                size[1] = h;
            }
        }
        break;
    case GL_WINDOW_POSITION_CR:
        /* return window position, as a screen coordinate */
        {
            GLint *pos = (GLint *) values;
            GLint x, y, w, h;
            WindowInfo *window;
            CRASSERT(type == GL_INT);
            CRASSERT(count == 2);
            CRASSERT(values);
            pos[0] = pos[1] = 0;  /* default */
            window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, index);
            if (window)
            {
                renderspu_SystemGetWindowGeometry(window, &x, &y, &w, &h);
                pos[0] = x;/*window->x;*/
                pos[1] = y;/*window->y;*/
            }
        }
        break;
    case GL_MAX_WINDOW_SIZE_CR:
        {
            GLint *maxSize = (GLint *) values;
            WindowInfo *window;
            CRASSERT(type == GL_INT);
            CRASSERT(count == 2);
            CRASSERT(values);
            window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, index);
            if (window)
            {
                renderspu_SystemGetMaxWindowSize(window, maxSize + 0, maxSize + 1);
            }
        }
        break;
    case GL_WINDOW_VISIBILITY_CR:
        {
            GLint *vis = (GLint *) values;
            WindowInfo *window;
            CRASSERT(type == GL_INT);
            CRASSERT(count == 1);
            CRASSERT(values);
            vis[0] = 0;  /* default */
            window = (WindowInfo *) crHashtableSearch(render_spu.windowTable, index);
            if (window)
            {
                vis[0] = window->visible;
            }
        }
        break;
    default:
        ; /* nothing - silence compiler */
    }
}


static void RENDER_APIENTRY
renderspuBoundsInfoCR( CRrecti *bounds, GLbyte *payload, GLint len,
                                             GLint num_opcodes )
{
    (void) bounds;
    (void) payload;
    (void) len;
    (void) num_opcodes;
    /* draw the bounding box */
    if (render_spu.draw_bbox) {
        GET_CONTEXT(context);
        WindowInfo *window = context->currentWindow;
        GLint x, y, w, h;

        renderspu_SystemGetWindowGeometry(window, &x, &y, &w, &h);

        render_spu.self.PushMatrix();
        render_spu.self.LoadIdentity();
        render_spu.self.MatrixMode(GL_PROJECTION);
        render_spu.self.PushMatrix();
        render_spu.self.LoadIdentity();
        render_spu.self.Ortho(0, w, 0, h, -1, 1);
        render_spu.self.Color3f(1, 1, 1);
        render_spu.self.Begin(GL_LINE_LOOP);
        render_spu.self.Vertex2i(bounds->x1, bounds->y1);
        render_spu.self.Vertex2i(bounds->x2, bounds->y1);
        render_spu.self.Vertex2i(bounds->x2, bounds->y2);
        render_spu.self.Vertex2i(bounds->x1, bounds->y2);
        render_spu.self.End();
        render_spu.self.PopMatrix();
        render_spu.self.MatrixMode(GL_MODELVIEW);
        render_spu.self.PopMatrix();
    }
}


static void RENDER_APIENTRY
renderspuWriteback( GLint *writeback )
{
    (void) writeback;
}


static void
remove_trailing_space(char *s)
{
    int k = crStrlen(s);
    while (k > 0 && s[k-1] == ' ')
        k--;
    s[k] = 0;
}

static const GLubyte * RENDER_APIENTRY
renderspuGetString(GLenum pname)
{
    static char tempStr[1000];
    GET_CONTEXT(context);

    if (pname == GL_EXTENSIONS)
    {
        const char *nativeExt;
        char *crExt, *s1, *s2;

        if (!render_spu.ws.glGetString)
            return NULL;

        nativeExt = (const char *) render_spu.ws.glGetString(GL_EXTENSIONS);
        if (!nativeExt) {
            /* maybe called w/out current context. */
            return NULL;
        }

        crExt = crStrjoin3(crExtensions, " ", crAppOnlyExtensions);
        s1 = crStrIntersect(nativeExt, crExt);
        remove_trailing_space(s1);
        s2 = crStrjoin3(s1, " ", crChromiumExtensions);
        remove_trailing_space(s2);
        crFree(crExt);
        crFree(s1);
        if (context->extensionString)
            crFree(context->extensionString);
        context->extensionString = s2;
        return (const GLubyte *) s2;
    }
    else if (pname == GL_VENDOR)
        return (const GLubyte *) CR_VENDOR;
    else if (pname == GL_VERSION)
        return render_spu.ws.glGetString(GL_VERSION);
    else if (pname == GL_RENDERER) {
#ifdef VBOX
        snprintf(tempStr, sizeof(tempStr), "Chromium (%s)", (char *) render_spu.ws.glGetString(GL_RENDERER));
#else
        sprintf(tempStr, "Chromium (%s)", (char *) render_spu.ws.glGetString(GL_RENDERER));
#endif
        return (const GLubyte *) tempStr;
    }
#ifdef CR_OPENGL_VERSION_2_0
    else if (pname == GL_SHADING_LANGUAGE_VERSION)
        return render_spu.ws.glGetString(GL_SHADING_LANGUAGE_VERSION);
#endif
#ifdef GL_CR_real_vendor_strings
    else if (pname == GL_REAL_VENDOR)
        return render_spu.ws.glGetString(GL_VENDOR);
    else if (pname == GL_REAL_VERSION)
        return render_spu.ws.glGetString(GL_VERSION);
    else if (pname == GL_REAL_RENDERER)
        return render_spu.ws.glGetString(GL_RENDERER);
    else if (pname == GL_REAL_EXTENSIONS)
        return render_spu.ws.glGetString(GL_EXTENSIONS);
#endif
    else
        return NULL;
}

DECLEXPORT(void) renderspuReparentWindow(GLint window)
{
    WindowInfo *pWindow;
    CRASSERT(window >= 0);

    pWindow = (WindowInfo *) crHashtableSearch(render_spu.windowTable, window);

    if (!pWindow)
    {
        crDebug("Render SPU: Attempt to reparent invalid window (%d)", window);
        return;
    }

    renderspu_SystemReparentWindow(pWindow);
}

#define FILLIN( NAME, FUNC ) \
  table[i].name = crStrdup(NAME); \
  table[i].fn = (SPUGenericFunction) FUNC; \
  i++;


/* These are the functions which the render SPU implements, not OpenGL.
 */
int
renderspuCreateFunctions(SPUNamedFunctionTable table[])
{
    int i = 0;
    FILLIN( "SwapBuffers", renderspuSwapBuffers );
    FILLIN( "CreateContext", renderspuCreateContext );
    FILLIN( "DestroyContext", renderspuDestroyContext );
    FILLIN( "MakeCurrent", renderspuMakeCurrent );
    FILLIN( "WindowCreate", renderspuWindowCreate );
    FILLIN( "WindowDestroy", renderspuWindowDestroy );
    FILLIN( "WindowSize", renderspuWindowSize );
    FILLIN( "WindowPosition", renderspuWindowPosition );
    FILLIN( "WindowVisibleRegion", renderspuWindowVisibleRegion );
    FILLIN( "WindowShow", renderspuWindowShow );
    FILLIN( "BarrierCreateCR", renderspuBarrierCreateCR );
    FILLIN( "BarrierDestroyCR", renderspuBarrierDestroyCR );
    FILLIN( "BarrierExecCR", renderspuBarrierExecCR );
    FILLIN( "BoundsInfoCR", renderspuBoundsInfoCR );
    FILLIN( "SemaphoreCreateCR", renderspuSemaphoreCreateCR );
    FILLIN( "SemaphoreDestroyCR", renderspuSemaphoreDestroyCR );
    FILLIN( "SemaphorePCR", renderspuSemaphorePCR );
    FILLIN( "SemaphoreVCR", renderspuSemaphoreVCR );
    FILLIN( "Writeback", renderspuWriteback );
    FILLIN( "ChromiumParameteriCR", renderspuChromiumParameteriCR );
    FILLIN( "ChromiumParameterfCR", renderspuChromiumParameterfCR );
    FILLIN( "ChromiumParametervCR", renderspuChromiumParametervCR );
    FILLIN( "GetChromiumParametervCR", renderspuGetChromiumParametervCR );
    FILLIN( "GetString", renderspuGetString );
    FILLIN( "VBoxPresentComposition", renderspuVBoxPresentComposition );

    return i;
}

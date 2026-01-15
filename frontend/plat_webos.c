/*
 * WebOS platform support for PCSX-ReARMed
 *
 * This file handles Palm Device Library (PDL) initialization
 * required for GPU-accelerated rendering on WebOS devices.
 *
 * WebOS PDK apps need to call PDL_Init() before SDL_Init()
 * to properly integrate with the system and enable hardware
 * GPU access through OpenGL ES.
 */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

static void *libpdl;
static int pdl_initialized;

/* PDL function pointers */
typedef int (*PDL_Init_t)(unsigned int flags);
typedef void (*PDL_Quit_t)(void);
typedef const char* (*PDL_GetError_t)(void);
typedef int (*PDL_NotifyMusicPlaying_t)(int playing);
typedef int (*PDL_SetTouchAggression_t)(int aggression);

static PDL_Init_t pPDL_Init;
static PDL_Quit_t pPDL_Quit;
static PDL_GetError_t pPDL_GetError;
static PDL_NotifyMusicPlaying_t pPDL_NotifyMusicPlaying;
static PDL_SetTouchAggression_t pPDL_SetTouchAggression;

/* PDL touch aggression levels */
#define PDL_AGGRESSION_LESSTOUCHES 1
#define PDL_AGGRESSION_MORETOUCHES 2

/*
 * Initialize PDL (Palm Device Library)
 * Must be called before SDL_Init() on WebOS
 * Returns 0 on success, -1 on failure
 */
int webos_init(void)
{
    if (pdl_initialized)
        return 0;

    /* Try to load PDL library */
    libpdl = dlopen("libpdl.so", RTLD_LAZY);
    if (!libpdl) {
        fprintf(stderr, "WebOS: libpdl.so not found - not running on WebOS?\n");
        return -1;
    }

    /* Load required functions */
    pPDL_Init = (PDL_Init_t)dlsym(libpdl, "PDL_Init");
    pPDL_Quit = (PDL_Quit_t)dlsym(libpdl, "PDL_Quit");
    pPDL_GetError = (PDL_GetError_t)dlsym(libpdl, "PDL_GetError");
    pPDL_NotifyMusicPlaying = (PDL_NotifyMusicPlaying_t)dlsym(libpdl, "PDL_NotifyMusicPlaying");
    pPDL_SetTouchAggression = (PDL_SetTouchAggression_t)dlsym(libpdl, "PDL_SetTouchAggression");

    if (!pPDL_Init) {
        fprintf(stderr, "WebOS: Failed to load PDL_Init\n");
        dlclose(libpdl);
        libpdl = NULL;
        return -1;
    }

    /* Initialize PDL with no flags */
    printf("WebOS: Initializing PDL...\n");
    if (pPDL_Init(0) != 0) {
        const char *err = pPDL_GetError ? pPDL_GetError() : "unknown error";
        fprintf(stderr, "WebOS: PDL_Init failed: %s\n", err);
        dlclose(libpdl);
        libpdl = NULL;
        return -1;
    }

    printf("WebOS: PDL initialized successfully\n");
    pdl_initialized = 1;

    /* Set touch aggression to reduce flicker on touch events */
    if (pPDL_SetTouchAggression) {
        pPDL_SetTouchAggression(PDL_AGGRESSION_MORETOUCHES);
        printf("WebOS: Touch aggression set to MORETOUCHES\n");
    }

    return 0;
}

/*
 * Clean up PDL resources
 */
void webos_finish(void)
{
    if (!pdl_initialized)
        return;

    if (pPDL_Quit)
        pPDL_Quit();

    if (libpdl) {
        dlclose(libpdl);
        libpdl = NULL;
    }

    pdl_initialized = 0;
}

/*
 * Notify system about audio playback state
 * Prevents system from suspending during playback
 */
void webos_notify_music(int playing)
{
    if (pdl_initialized && pPDL_NotifyMusicPlaying)
        pPDL_NotifyMusicPlaying(playing);
}

/*
 * Check if running on WebOS
 */
int webos_is_available(void)
{
    return pdl_initialized;
}

/*
 * Set default video output to OpenGL via SDL (not EGL)
 * This should be called after plat_init() to override any config defaults
 *
 * Note: We use SDL's OpenGL support (like TuxRacer) instead of EGL directly.
 * EGL doesn't work properly on WebOS and causes screen flicker on touch events.
 * SDL manages the GL context internally and integrates with WebOS's 3-layer system.
 */
void webos_set_video_default(void)
{
    if (!pdl_initialized)
        return;

    /* Call the platform function to set GL mode */
    extern void plat_sdl_set_gl_default(void);

    printf("WebOS: Setting OpenGL (via SDL) as default output for GPU acceleration\n");
    plat_sdl_set_gl_default();
}

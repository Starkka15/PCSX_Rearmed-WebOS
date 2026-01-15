/*
 * WebOS platform support for PCSX-ReARMed
 */

#ifndef __PLAT_WEBOS_H__
#define __PLAT_WEBOS_H__

#ifdef WEBOS

/* Initialize PDL - must be called before SDL_Init() */
int webos_init(void);

/* Clean up PDL resources */
void webos_finish(void);

/* Notify system about audio playback state */
void webos_notify_music(int playing);

/* Check if running on WebOS */
int webos_is_available(void);

/* Set Video Overlay as default video output (hardware accelerated, avoids touch flicker) */
void webos_set_video_default(void);

#else

/* Stubs for non-WebOS builds */
static inline int webos_init(void) { return 0; }
static inline void webos_finish(void) { }
static inline void webos_notify_music(int playing) { (void)playing; }
static inline int webos_is_available(void) { return 0; }
static inline void webos_set_video_default(void) { }

#endif /* WEBOS */

#endif /* __PLAT_WEBOS_H__ */

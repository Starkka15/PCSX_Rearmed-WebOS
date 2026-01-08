/*
 * OpenGL ES 2.0 rendering for webOS TouchPad
 */

#ifndef GL_WEBOS_H
#define GL_WEBOS_H

#ifdef WEBOS_TOUCHPAD

int  gl_webos_init(void);
int  gl_webos_create(int w, int h);
void gl_webos_update_vertices(int src_w, int src_h, int dst_w, int dst_h,
                               int layer_x, int layer_y, int layer_w, int layer_h);
int  gl_webos_flip(const void *fb, int w, int h);
void gl_webos_clear(void);
void gl_webos_destroy(void);
void gl_webos_shutdown(void);
int  gl_webos_is_active(void);

#else

/* Stubs for non-webOS builds */
static inline int gl_webos_init(void) { return -1; }
static inline int gl_webos_create(int w, int h) { return -1; }
static inline void gl_webos_update_vertices(int sw, int sh, int dw, int dh,
                                             int lx, int ly, int lw, int lh) {}
static inline int gl_webos_flip(const void *fb, int w, int h) { return -1; }
static inline void gl_webos_clear(void) {}
static inline void gl_webos_destroy(void) {}
static inline void gl_webos_shutdown(void) {}
static inline int gl_webos_is_active(void) { return 0; }

#endif

#endif /* GL_WEBOS_H */

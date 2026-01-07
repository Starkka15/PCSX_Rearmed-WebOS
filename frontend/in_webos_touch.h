/*
 * webOS TouchPad on-screen controls
 */

#ifndef IN_WEBOS_TOUCH_H
#define IN_WEBOS_TOUCH_H

#include <SDL/SDL.h>

void in_webos_touch_init(void);
void in_webos_touch_event(int x, int y, int pressed);
void in_webos_touch_event_finger(int finger_id, int x, int y, int pressed);
void in_webos_touch_draw_overlay(SDL_Surface *screen);
void in_webos_touch_draw_overlay_yuv(SDL_Overlay *overlay, int dest_w, int dest_h);
void in_webos_touch_draw_overlay_yuv_borders(SDL_Overlay *overlay, int dest_w, int dest_h,
                                              int video_x, int video_y, int video_w, int video_h);
void in_webos_touch_draw_overlay_borders(SDL_Surface *screen,
                                          int video_x, int video_y, int video_w, int video_h);

#endif

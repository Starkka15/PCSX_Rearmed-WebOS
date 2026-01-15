/*
 * WebOS touchscreen input driver with on-screen controls overlay
 * For HP TouchPad (1024x768)
 * Uses SDL 2D surface drawing (works without GL)
 */
#ifdef WEBOS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "libpicofe/input.h"
#include "plugin_lib.h"
#include "in_webos_touch.h"

#define TOUCH_SCREEN_W 1024
#define TOUCH_SCREEN_H 768

/* Touch zone definitions */
typedef struct {
    int x, y, w, h;
    int key;           /* DKEY_* value */
    const char *label;
} touch_zone_t;

/* Layout for 1024x768 landscape */
static const touch_zone_t touch_zones[] = {
    /* D-Pad - left side */
    { 80,  280, 80, 80,  DKEY_UP,       "UP" },
    { 80,  440, 80, 80,  DKEY_DOWN,     "DN" },
    { 0,   360, 80, 80,  DKEY_LEFT,     "LT" },
    { 160, 360, 80, 80,  DKEY_RIGHT,    "RT" },

    /* Action buttons - right side (PlayStation layout) */
    { 864, 280, 80, 80,  DKEY_TRIANGLE, "/\\" },
    { 864, 440, 80, 80,  DKEY_CROSS,    "X" },
    { 784, 360, 80, 80,  DKEY_SQUARE,   "[]" },
    { 944, 360, 80, 80,  DKEY_CIRCLE,   "O" },

    /* Shoulder buttons - top corners */
    { 0,   0,   120, 60, DKEY_L1,       "L1" },
    { 0,   60,  120, 60, DKEY_L2,       "L2" },
    { 904, 0,   120, 60, DKEY_R1,       "R1" },
    { 904, 60,  120, 60, DKEY_R2,       "R2" },

    /* Start/Select - bottom center */
    { 400, 700, 100, 60, DKEY_SELECT,   "SEL" },
    { 524, 700, 100, 60, DKEY_START,    "STA" },

    /* Menu button - top center */
    { 462, 0,   100, 50, -1,            "MENU" },  /* -1 = special: open menu */
};

#define NUM_TOUCH_ZONES (sizeof(touch_zones) / sizeof(touch_zones[0]))

/* Track which fingers are pressing which zones */
#define MAX_FINGERS 10
static int finger_zones[MAX_FINGERS];
static int current_buttons = 0;
static int overlay_visible = 1;
static int initialized = 0;

/* Track current screen dimensions for coordinate scaling */
static int current_screen_w = TOUCH_SCREEN_W;
static int current_screen_h = TOUCH_SCREEN_H;

/* Colors for overlay (RGB565) */
#define COLOR_BUTTON_NORMAL   0x4208  /* Dark gray */
#define COLOR_BUTTON_PRESSED  0x841F  /* Blue-ish */
#define COLOR_BUTTON_BORDER   0xFFFF  /* White */

/* Draw a filled rectangle to an SDL surface */
static void draw_rect_sdl(SDL_Surface *surface, int x, int y, int w, int h, Uint16 color)
{
    SDL_Rect rect = { x, y, w, h };
    SDL_FillRect(surface, &rect, color);
}

/* Draw a rectangle outline to an SDL surface */
static void draw_rect_outline_sdl(SDL_Surface *surface, int x, int y, int w, int h, Uint16 color, int thickness)
{
    /* Top */
    draw_rect_sdl(surface, x, y, w, thickness, color);
    /* Bottom */
    draw_rect_sdl(surface, x, y + h - thickness, w, thickness, color);
    /* Left */
    draw_rect_sdl(surface, x, y, thickness, h, color);
    /* Right */
    draw_rect_sdl(surface, x + w - thickness, y, thickness, h, color);
}

void webos_touch_draw_overlay(void)
{
    /* This function is no longer used for drawing - kept for API compatibility */
    /* Drawing is now done via webos_touch_draw_overlay_sdl */
}

static int debug_once = 0;

void webos_touch_draw_overlay_sdl(SDL_Surface *screen)
{
    int i;
    float scale_x, scale_y;
    int screen_w, screen_h;

    if (!overlay_visible || !initialized || !screen)
        return;

    screen_w = screen->w;
    screen_h = screen->h;

    if (!debug_once) {
        printf("WebOS Touch: Drawing overlay on %dx%d surface (target %dx%d)\n",
               screen_w, screen_h, TOUCH_SCREEN_W, TOUCH_SCREEN_H);
        debug_once = 1;
    }

    /* Store screen dimensions for touch coordinate scaling */
    current_screen_w = screen_w;
    current_screen_h = screen_h;

    /* Calculate scaling factors from touch coordinates (1024x768) to screen surface */
    scale_x = (float)screen_w / TOUCH_SCREEN_W;
    scale_y = (float)screen_h / TOUCH_SCREEN_H;

    if (SDL_MUSTLOCK(screen))
        SDL_LockSurface(screen);

    for (i = 0; i < NUM_TOUCH_ZONES; i++) {
        const touch_zone_t *zone = &touch_zones[i];
        int pressed = 0;
        int j;
        Uint16 bg_color;
        int draw_x, draw_y, draw_w, draw_h;

        /* Check if any finger is on this zone */
        for (j = 0; j < MAX_FINGERS; j++) {
            if (finger_zones[j] == i) {
                pressed = 1;
                break;
            }
        }

        bg_color = pressed ? COLOR_BUTTON_PRESSED : COLOR_BUTTON_NORMAL;

        /* Scale touch zone coordinates to screen surface */
        draw_x = (int)(zone->x * scale_x);
        draw_y = (int)(zone->y * scale_y);
        draw_w = (int)(zone->w * scale_x);
        draw_h = (int)(zone->h * scale_y);

        /* Draw button background */
        draw_rect_sdl(screen, draw_x, draw_y, draw_w, draw_h, bg_color);

        /* Draw border (scaled thickness) */
        draw_rect_outline_sdl(screen, draw_x, draw_y, draw_w, draw_h, COLOR_BUTTON_BORDER, 2);
    }

    if (SDL_MUSTLOCK(screen))
        SDL_UnlockSurface(screen);
}

static int find_zone(int x, int y)
{
    int i;
    int scaled_x, scaled_y;

    /* Scale touch coordinates from screen surface to touch zone coordinate space (1024x768) */
    scaled_x = (x * TOUCH_SCREEN_W) / current_screen_w;
    scaled_y = (y * TOUCH_SCREEN_H) / current_screen_h;

    for (i = 0; i < NUM_TOUCH_ZONES; i++) {
        const touch_zone_t *zone = &touch_zones[i];
        if (scaled_x >= zone->x && scaled_x < zone->x + zone->w &&
            scaled_y >= zone->y && scaled_y < zone->y + zone->h) {
            return i;
        }
    }
    return -1;
}

static void update_buttons(void)
{
    int i;
    current_buttons = 0;

    for (i = 0; i < MAX_FINGERS; i++) {
        if (finger_zones[i] >= 0) {
            int key = touch_zones[finger_zones[i]].key;
            if (key >= 0) {
                current_buttons |= (1 << key);
            }
        }
    }
}

/* Process SDL touch/mouse events */
int webos_touch_event(const SDL_Event *event)
{
    int finger_id;
    int x, y;
    int zone;

    if (!initialized)
        return 0;

    switch (event->type) {
    case SDL_MOUSEBUTTONDOWN:
        finger_id = 0;  /* Mouse is finger 0 */
        x = event->button.x;
        y = event->button.y;
        zone = find_zone(x, y);

        if (zone >= 0) {
            /* Check for menu button */
            if (touch_zones[zone].key == -1) {
                /* Return special value to indicate menu request */
                return 2;
            }
            finger_zones[finger_id] = zone;
            update_buttons();
        }
        return 1;

    case SDL_MOUSEBUTTONUP:
        finger_id = 0;
        finger_zones[finger_id] = -1;
        update_buttons();
        return 1;

    case SDL_MOUSEMOTION:
        if (event->motion.state & SDL_BUTTON(1)) {
            finger_id = 0;
            x = event->motion.x;
            y = event->motion.y;
            zone = find_zone(x, y);
            finger_zones[finger_id] = zone;
            update_buttons();
        }
        return 1;

    default:
        break;
    }

    return 0;
}

int webos_touch_get_buttons(void)
{
    return current_buttons;
}

void webos_touch_set_overlay_visible(int visible)
{
    overlay_visible = visible;
}

int webos_touch_init(void)
{
    int i;

    printf("WebOS Touch: Initializing on-screen controls\n");

    for (i = 0; i < MAX_FINGERS; i++) {
        finger_zones[i] = -1;
    }

    current_buttons = 0;
    overlay_visible = 1;
    initialized = 1;

    printf("WebOS Touch: %d touch zones defined\n", (int)NUM_TOUCH_ZONES);
    return 0;
}

void webos_touch_finish(void)
{
    initialized = 0;
}

#endif /* WEBOS */

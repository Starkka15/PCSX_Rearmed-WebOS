/*
 * webOS TouchPad on-screen controls
 * Maps touch zones to PSX controller buttons
 */

#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>
#include <PDL.h>
#include "libpicofe/input.h"
#include "libpicofe/fonts.h"
#include "plugin_lib.h"

// Default binds for in-game controls
// Maps touch zone IDs to PSX controller buttons
static const struct in_default_bind in_webos_touch_defbinds[] = {
	// D-pad
	{ 0,  IN_BINDTYPE_PLAYER12, DKEY_UP },
	{ 1,  IN_BINDTYPE_PLAYER12, DKEY_DOWN },
	{ 2,  IN_BINDTYPE_PLAYER12, DKEY_LEFT },
	{ 3,  IN_BINDTYPE_PLAYER12, DKEY_RIGHT },
	// Face buttons
	{ 4,  IN_BINDTYPE_PLAYER12, DKEY_TRIANGLE },
	{ 5,  IN_BINDTYPE_PLAYER12, DKEY_CIRCLE },
	{ 6,  IN_BINDTYPE_PLAYER12, DKEY_CROSS },
	{ 7,  IN_BINDTYPE_PLAYER12, DKEY_SQUARE },
	// Triggers
	{ 8,  IN_BINDTYPE_PLAYER12, DKEY_L1 },
	{ 9,  IN_BINDTYPE_PLAYER12, DKEY_R1 },
	{ 10, IN_BINDTYPE_PLAYER12, DKEY_L2 },
	{ 11, IN_BINDTYPE_PLAYER12, DKEY_R2 },
	// Start/Select
	{ 12, IN_BINDTYPE_PLAYER12, DKEY_START },
	{ 13, IN_BINDTYPE_PLAYER12, DKEY_SELECT },
	{ 0, 0, 0 }  // Terminator
};

// Menu button definitions from libpicofe/input.h
#define PBTN_UP    (1 <<  0)
#define PBTN_DOWN  (1 <<  1)
#define PBTN_LEFT  (1 <<  2)
#define PBTN_RIGHT (1 <<  3)
#define PBTN_MOK   (1 <<  4)
#define PBTN_MBACK (1 <<  5)
#define PBTN_L     (1 <<  8)
#define PBTN_R     (1 <<  9)
#define PBTN_MENU  (1 << 10)

#define IN_WEBOS_PREFIX "webos_touch:"
#define WEBOS_TOUCH_COUNT 16
#define array_size(x) (sizeof(x) / sizeof((x)[0]))

// Multitouch support - track up to 10 simultaneous touch points
#define MAX_TOUCH_POINTS 10
static struct {
	int x, y;
	int active;
	int button_id;
} touch_points[MAX_TOUCH_POINTS];

static unsigned int buttons_pressed = 0;      // Bitmask of currently pressed buttons
static unsigned int buttons_prev = 0;         // Previous frame's pressed buttons
static unsigned int buttons_just_pressed = 0; // Buttons pressed this frame
static unsigned int buttons_just_released = 0;// Buttons released this frame
static int screen_w = 1024, screen_h = 768;
static SDL_Surface *overlay_screen = NULL;

// PSX button mapping to touch zones
static const char * const webos_touch_keys[WEBOS_TOUCH_COUNT] = {
	"DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",  // 0-3
	"TRIANGLE", "CIRCLE", "CROSS", "SQUARE",             // 4-7
	"L1", "R1", "L2", "R2",                              // 8-11
	"START", "SELECT", "UNUSED1", "UNUSED2"              // 12-15
};

// Touch zone definitions (x1, y1, x2, y2) as percentages
typedef struct {
	int x1, y1, x2, y2;
	int button_id;
} touch_zone_t;

static touch_zone_t touch_zones[] = {
	// Left side - D-pad in diamond pattern (non-overlapping)
	//     UP
	//  LEFT  RIGHT
	//    DOWN
	{  3, 28, 12, 42, 0 },  // UP     (center-top)
	{  3, 58, 12, 72, 1 },  // DOWN   (center-bottom)
	{  0, 42,  7, 58, 2 },  // LEFT   (left-middle)
	{  8, 42, 15, 58, 3 },  // RIGHT  (right-middle)

	// Right side - Face buttons in diamond pattern
	//    TRIANGLE
	// SQUARE  CIRCLE
	//    CROSS
	{ 88, 28, 97, 42, 4 },  // TRIANGLE (center-top)
	{ 93, 42, 100, 58, 5 }, // CIRCLE   (right-middle)
	{ 88, 58, 97, 72, 6 },  // CROSS    (center-bottom)
	{ 85, 42, 92, 58, 7 },  // SQUARE   (left-middle)

	// Top triggers (shoulder buttons)
	{  0,  0, 20, 12, 8 },  // L1
	{ 80,  0, 100, 12, 9 }, // R1
	{  0, 12, 20, 24, 10 }, // L2
	{ 80, 12, 100, 24, 11 }, // R2

	// Bottom center - Start/Select
	{ 35, 88, 48, 100, 12 }, // START
	{ 52, 88, 65, 100, 13 }, // SELECT
};

static void in_webos_touch_probe(const in_drv_t *drv)
{
	SDL_version sdl_version;
	SDL_VERSION(&sdl_version);

	printf("webOS Touch: Initializing on-screen controls (SDL %d.%d.%d)\n",
		sdl_version.major, sdl_version.minor, sdl_version.patch);

	// Register as async device (drv_fd_hnd = -1)
	// Now with proper update_keycode and menu_translate implementations
	in_register(IN_WEBOS_PREFIX "webos touchscreen", -1, NULL,
		WEBOS_TOUCH_COUNT, webos_touch_keys, 0);
}

static const char * const *in_webos_touch_get_key_names(const in_drv_t *drv, int *count)
{
	*count = WEBOS_TOUCH_COUNT;
	return webos_touch_keys;
}

static int get_touched_button(int x, int y)
{
	int i;
	int px = (x * 100) / screen_w;
	int py = (y * 100) / screen_h;

	for (i = 0; i < (int)array_size(touch_zones); i++) {
		if (px >= touch_zones[i].x1 && px < touch_zones[i].x2 &&
		    py >= touch_zones[i].y1 && py < touch_zones[i].y2) {
			return touch_zones[i].button_id;
		}
	}

	return -1;
}

// Recalculate buttons_pressed from all active touch points
static void update_buttons_state(void)
{
	int i;
	unsigned int new_pressed = 0;

	for (i = 0; i < MAX_TOUCH_POINTS; i++) {
		if (touch_points[i].active && touch_points[i].button_id >= 0) {
			new_pressed |= (1 << touch_points[i].button_id);
		}
	}

	// Calculate just pressed/released (compare against current buttons_pressed, not prev)
	buttons_just_pressed = new_pressed & ~buttons_pressed;
	buttons_just_released = buttons_pressed & ~new_pressed;
	buttons_prev = buttons_pressed;
	buttons_pressed = new_pressed;
}

// Handle touch event with finger ID for multitouch
void in_webos_touch_event_finger(int finger_id, int x, int y, int pressed)
{
	if (finger_id < 0 || finger_id >= MAX_TOUCH_POINTS)
		finger_id = 0;

	if (pressed) {
		touch_points[finger_id].x = x;
		touch_points[finger_id].y = y;
		touch_points[finger_id].active = 1;
		touch_points[finger_id].button_id = get_touched_button(x, y);
	} else {
		touch_points[finger_id].active = 0;
		touch_points[finger_id].button_id = -1;
	}

	update_buttons_state();
}

// Legacy single-touch handler (for compatibility)
void in_webos_touch_event(int x, int y, int pressed)
{
	in_webos_touch_event_finger(0, x, y, pressed);
}

static int in_webos_touch_update(void *drv_data, const int *binds, int *result)
{
	int i;

	// Apply binds for ALL currently pressed buttons (multitouch)
	for (i = 0; i < WEBOS_TOUCH_COUNT; i++) {
		if (buttons_pressed & (1 << i)) {
			int t;
			for (t = 0; t < IN_BINDTYPE_COUNT; t++)
				result[t] |= binds[IN_BIND_OFFS(i, t)];
		}
	}

	return 0;
}

// Menu input polling - return keycode or -1 if no input
// For menu, we report one button at a time (press/release events)
static int in_webos_touch_update_keycode(void *drv_data, int *is_down)
{
	static unsigned int reported_pressed = 0;  // Buttons we've reported as pressed
	int i;

	// First, report any released buttons
	for (i = 0; i < WEBOS_TOUCH_COUNT; i++) {
		if ((reported_pressed & (1 << i)) && !(buttons_pressed & (1 << i))) {
			reported_pressed &= ~(1 << i);
			if (is_down)
				*is_down = 0;
			return i;
		}
	}

	// Then, report any newly pressed buttons
	for (i = 0; i < WEBOS_TOUCH_COUNT; i++) {
		if ((buttons_pressed & (1 << i)) && !(reported_pressed & (1 << i))) {
			reported_pressed |= (1 << i);
			if (is_down)
				*is_down = 1;
			return i;
		}
	}

	return -1;  // No new input
}

// Translate touch buttons to menu button codes
static int in_webos_touch_menu_translate(void *drv_data, int keycode, char *charcode)
{
	// Map touch buttons to menu actions
	switch (keycode) {
	case 0:  return PBTN_UP;      // DPAD_UP
	case 1:  return PBTN_DOWN;    // DPAD_DOWN
	case 2:  return PBTN_LEFT;    // DPAD_LEFT
	case 3:  return PBTN_RIGHT;   // DPAD_RIGHT
	case 6:  return PBTN_MOK;     // CROSS (confirm)
	case 5:  return PBTN_MBACK;   // CIRCLE (back)
	case 8:  return PBTN_L;       // L1
	case 9:  return PBTN_R;       // R1
	case 12: return PBTN_MENU;    // START
	default: return 0;
	}
}

static const in_drv_t in_webos_touch_drv = {
	.prefix         = IN_WEBOS_PREFIX,
	.probe          = in_webos_touch_probe,
	.get_key_names  = in_webos_touch_get_key_names,
	.update         = in_webos_touch_update,
	.update_keycode = in_webos_touch_update_keycode,
	.menu_translate = in_webos_touch_menu_translate,
};

void in_webos_touch_init(void)
{
	PDL_Err err;

	printf("webOS Touch: Initializing PDL for multitouch support\n");

	// Initialize PDL
	err = PDL_Init(0);
	if (err != PDL_NOERROR) {
		printf("webOS Touch: PDL_Init failed: %s\n", PDL_GetError());
	} else {
		// Enable multitouch - MORE likely to see multi-touches
		err = PDL_SetTouchAggression(PDL_AGGRESSION_MORETOUCHES);
		if (err != PDL_NOERROR) {
			printf("webOS Touch: PDL_SetTouchAggression failed: %s\n", PDL_GetError());
		} else {
			printf("webOS Touch: Multitouch enabled (PDL_AGGRESSION_MORETOUCHES)\n");
		}
	}

	printf("webOS Touch: Registering input driver with default binds\n");
	in_register_driver(&in_webos_touch_drv, in_webos_touch_defbinds, NULL, NULL);
}

// Button labels for overlay (for future text rendering)
static const char *button_labels[] __attribute__((unused)) = {
	"UP", "DN", "LF", "RT",          // D-pad
	"TRI", "O", "X", "SQ",           // Face buttons
	"L1", "R1", "L2", "R2",          // Triggers
	"STA", "SEL", "", ""
};

// Simple pixel drawing helper - draws a thick outlined rectangle
static void draw_rect_outline(SDL_Surface *surface, int x, int y, int w, int h, Uint32 color, int thickness)
{
	SDL_Rect rect;

	if (w <= 0 || h <= 0)
		return;

	// Top
	rect.x = x; rect.y = y; rect.w = w; rect.h = thickness;
	SDL_FillRect(surface, &rect, color);

	// Bottom
	rect.y = y + h - thickness;
	SDL_FillRect(surface, &rect, color);

	// Left
	rect.x = x; rect.y = y; rect.w = thickness; rect.h = h;
	SDL_FillRect(surface, &rect, color);

	// Right
	rect.x = x + w - thickness;
	SDL_FillRect(surface, &rect, color);
}

// Convert RGB to YUV (for UYVY overlay format)
static void rgb_to_yuv(int r, int g, int b, unsigned char *y, unsigned char *u, unsigned char *v)
{
	*y = (unsigned char)(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
	*u = (unsigned char)(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
	*v = (unsigned char)(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
}

// Draw a filled rectangle on YUV overlay (UYVY format)
static void draw_rect_yuv(unsigned char *pixels, int pitch, int surf_h,
                          int x, int y, int w, int h,
                          unsigned char yval, unsigned char uval, unsigned char vval)
{
	int row, col;
	unsigned char *line;

	if (w <= 0 || h <= 0)
		return;

	// Clamp to surface bounds
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > pitch / 2) w = pitch / 2 - x;
	if (y + h > surf_h) h = surf_h - y;
	if (w <= 0 || h <= 0)
		return;

	// UYVY format: each 4 bytes = U Y V Y (2 pixels)
	// pitch is in bytes, each pixel pair is 4 bytes
	for (row = y; row < y + h; row++) {
		line = pixels + row * pitch;
		for (col = x; col < x + w; col++) {
			int byte_offset = col * 2;  // 2 bytes per pixel in UYVY
			// For even pixels: write U, Y
			// For odd pixels: write V, Y
			if (col & 1) {
				line[byte_offset] = vval;     // V
				line[byte_offset + 1] = yval; // Y
			} else {
				line[byte_offset] = uval;     // U
				line[byte_offset + 1] = yval; // Y
			}
		}
	}
}

// Draw outline on YUV overlay
static void draw_rect_outline_yuv(unsigned char *pixels, int pitch, int surf_h,
                                   int x, int y, int w, int h,
                                   unsigned char yval, unsigned char uval, unsigned char vval,
                                   int thickness)
{
	if (w <= 0 || h <= 0)
		return;

	// Top
	draw_rect_yuv(pixels, pitch, surf_h, x, y, w, thickness, yval, uval, vval);
	// Bottom
	draw_rect_yuv(pixels, pitch, surf_h, x, y + h - thickness, w, thickness, yval, uval, vval);
	// Left
	draw_rect_yuv(pixels, pitch, surf_h, x, y, thickness, h, yval, uval, vval);
	// Right
	draw_rect_yuv(pixels, pitch, surf_h, x + w - thickness, y, thickness, h, yval, uval, vval);
}

// Draw touch controls on YUV overlay (called when using hardware overlay)
// Note: dest_w/dest_h are the overlay dimensions, touch zones are scaled to fit
void in_webos_touch_draw_overlay_yuv(SDL_Overlay *overlay, int dest_w, int dest_h)
{
	int i;
	unsigned char y_outline, u_outline, v_outline;
	unsigned char y_pressed, u_pressed, v_pressed;
	int thickness = 2;
	unsigned char *pixels;
	int pitch;
	int draw_w = dest_w;
	int draw_h = dest_h;
	static int debug_once = 1;

	if (!overlay || !overlay->pixels[0])
		return;

	// Debug output once
	if (debug_once) {
		printf("webOS Touch: Drawing on YUV overlay %dx%d, pitch=%d, format=%x\n",
		       overlay->w, overlay->h, overlay->pitches[0], overlay->format);
		debug_once = 0;
	}

	// Lock the overlay for direct pixel access
	if (SDL_LockYUVOverlay(overlay) < 0)
		return;

	pixels = (unsigned char *)overlay->pixels[0];
	pitch = overlay->pitches[0];

	// Convert colors to YUV
	rgb_to_yuv(255, 255, 255, &y_outline, &u_outline, &v_outline);  // White outline
	rgb_to_yuv(255, 255, 0, &y_pressed, &u_pressed, &v_pressed);    // Yellow when pressed

	// Draw all touch zones (scaled to overlay dimensions)
	for (i = 0; i < (int)array_size(touch_zones); i++) {
		int x1 = (touch_zones[i].x1 * draw_w) / 100;
		int y1 = (touch_zones[i].y1 * draw_h) / 100;
		int x2 = (touch_zones[i].x2 * draw_w) / 100;
		int y2 = (touch_zones[i].y2 * draw_h) / 100;
		int w = x2 - x1;
		int h = y2 - y1;
		int button_id = touch_zones[i].button_id;
		int is_pressed = (buttons_pressed & (1 << button_id)) != 0;
		const char *label = button_labels[button_id];
		int text_x, text_y;

		// Draw outline only (transparent interior)
		draw_rect_outline_yuv(pixels, pitch, overlay->h, x1, y1, w, h,
			is_pressed ? y_pressed : y_outline,
			is_pressed ? u_pressed : u_outline,
			is_pressed ? v_pressed : v_outline,
			is_pressed ? thickness + 1 : thickness);

		// Draw text label centered in the button
		if (label && label[0]) {
			int label_len = strlen(label);
			text_x = x1 + (w - label_len * 8) / 2;  // 8 pixels per char
			text_y = y1 + (h - 8) / 2;              // 8 pixel height
			if (text_x >= 0 && text_y >= 0) {
				basic_text_out_uyvy_nf(pixels, pitch / 2, text_x, text_y, label);
			}
		}
	}

	SDL_UnlockYUVOverlay(overlay);
}

// Draw touch controls only in border areas (outside game video)
// video_x, video_y, video_w, video_h define the game video rectangle in pixels
void in_webos_touch_draw_overlay_yuv_borders(SDL_Overlay *overlay, int dest_w, int dest_h,
                                              int video_x, int video_y, int video_w, int video_h)
{
	int i;
	unsigned char y_outline, u_outline, v_outline;
	unsigned char y_pressed, u_pressed, v_pressed;
	int thickness = 2;
	unsigned char *pixels;
	int pitch;
	int draw_w = dest_w;
	int draw_h = dest_h;

	// Convert video rect to percentages for comparison with touch zones
	int video_x1_pct = (video_x * 100) / dest_w;
	int video_y1_pct = (video_y * 100) / dest_h;
	int video_x2_pct = ((video_x + video_w) * 100) / dest_w;
	int video_y2_pct = ((video_y + video_h) * 100) / dest_h;

	if (!overlay || !overlay->pixels[0])
		return;

	// Lock the overlay for direct pixel access
	if (SDL_LockYUVOverlay(overlay) < 0)
		return;

	pixels = (unsigned char *)overlay->pixels[0];
	pitch = overlay->pitches[0];

	// Convert colors to YUV
	rgb_to_yuv(255, 255, 255, &y_outline, &u_outline, &v_outline);  // White outline
	rgb_to_yuv(255, 255, 0, &y_pressed, &u_pressed, &v_pressed);    // Yellow when pressed

	// Draw only touch zones that are completely outside the video area
	for (i = 0; i < (int)array_size(touch_zones); i++) {
		int zx1 = touch_zones[i].x1;
		int zy1 = touch_zones[i].y1;
		int zx2 = touch_zones[i].x2;
		int zy2 = touch_zones[i].y2;

		// Check if this zone overlaps with the video area
		// Zone overlaps if NOT (completely left OR completely right OR completely above OR completely below)
		int overlaps = !(zx2 <= video_x1_pct || zx1 >= video_x2_pct ||
		                 zy2 <= video_y1_pct || zy1 >= video_y2_pct);

		if (overlaps)
			continue;  // Skip this button - it would be over the video

		// Calculate pixel coordinates
		int x1 = (zx1 * draw_w) / 100;
		int y1 = (zy1 * draw_h) / 100;
		int x2 = (zx2 * draw_w) / 100;
		int y2 = (zy2 * draw_h) / 100;
		int w = x2 - x1;
		int h = y2 - y1;
		int button_id = touch_zones[i].button_id;
		int is_pressed = (buttons_pressed & (1 << button_id)) != 0;
		const char *label = button_labels[button_id];
		int text_x, text_y;

		// Draw outline only (transparent interior)
		draw_rect_outline_yuv(pixels, pitch, overlay->h, x1, y1, w, h,
			is_pressed ? y_pressed : y_outline,
			is_pressed ? u_pressed : u_outline,
			is_pressed ? v_pressed : v_outline,
			is_pressed ? thickness + 1 : thickness);

		// Draw text label centered in the button
		if (label && label[0]) {
			int label_len = strlen(label);
			text_x = x1 + (w - label_len * 8) / 2;
			text_y = y1 + (h - 8) / 2;
			if (text_x >= 0 && text_y >= 0) {
				basic_text_out_uyvy_nf(pixels, pitch / 2, text_x, text_y, label);
			}
		}
	}

	SDL_UnlockYUVOverlay(overlay);
}

// Draw touch controls on SDL surface (fallback for non-overlay modes)
void in_webos_touch_draw_overlay(SDL_Surface *screen)
{
	int i;
	Uint32 color_outline, color_pressed;
	int thickness = 2;
	static int debug_once = 1;

	if (!screen)
		return;

	// Debug output once
	if (debug_once) {
		printf("webOS Touch: Drawing on SDL surface %dx%d\n", screen->w, screen->h);
		debug_once = 0;
	}

	// Safety check: make sure surface is valid and accessible
	if (SDL_MUSTLOCK(screen)) {
		if (SDL_LockSurface(screen) < 0)
			return;
	}

	overlay_screen = screen;
	screen_w = screen->w;
	screen_h = screen->h;

	// Colors: white outline, yellow when pressed
	color_outline = SDL_MapRGB(screen->format, 255, 255, 255);  // White outline
	color_pressed = SDL_MapRGB(screen->format, 255, 255, 0);    // Yellow when pressed

	// Draw all touch zones
	for (i = 0; i < (int)array_size(touch_zones); i++) {
		int x1 = (touch_zones[i].x1 * screen_w) / 100;
		int y1 = (touch_zones[i].y1 * screen_h) / 100;
		int x2 = (touch_zones[i].x2 * screen_w) / 100;
		int y2 = (touch_zones[i].y2 * screen_h) / 100;
		int w = x2 - x1;
		int h = y2 - y1;
		int button_id = touch_zones[i].button_id;
		int is_pressed = (buttons_pressed & (1 << button_id)) != 0;
		const char *label = button_labels[button_id];
		int text_x, text_y;

		// Draw outline only (transparent interior)
		draw_rect_outline(screen, x1, y1, w, h,
			is_pressed ? color_pressed : color_outline,
			is_pressed ? thickness + 1 : thickness);

		// Draw text label centered in the button
		if (label && label[0] && screen->format->BitsPerPixel == 16) {
			int label_len = strlen(label);
			text_x = x1 + (w - label_len * 8) / 2;
			text_y = y1 + (h - 8) / 2;
			if (text_x >= 0 && text_y >= 0) {
				basic_text_out16_nf(screen->pixels, screen->pitch / 2, text_x, text_y, label);
			}
		}
	}

	if (SDL_MUSTLOCK(screen)) {
		SDL_UnlockSurface(screen);
	}
}

// Draw touch controls on SDL surface, only in border areas (outside game video)
void in_webos_touch_draw_overlay_borders(SDL_Surface *screen,
                                          int video_x, int video_y, int video_w, int video_h)
{
	int i;
	Uint32 color_outline, color_pressed;
	int thickness = 2;

	if (!screen)
		return;

	// Convert video rect to percentages for comparison with touch zones
	int video_x1_pct = (video_x * 100) / screen->w;
	int video_y1_pct = (video_y * 100) / screen->h;
	int video_x2_pct = ((video_x + video_w) * 100) / screen->w;
	int video_y2_pct = ((video_y + video_h) * 100) / screen->h;

	// Safety check: make sure surface is valid and accessible
	if (SDL_MUSTLOCK(screen)) {
		if (SDL_LockSurface(screen) < 0)
			return;
	}

	overlay_screen = screen;
	screen_w = screen->w;
	screen_h = screen->h;

	// Colors: white outline, yellow when pressed
	color_outline = SDL_MapRGB(screen->format, 255, 255, 255);
	color_pressed = SDL_MapRGB(screen->format, 255, 255, 0);

	// Draw only touch zones that are completely outside the video area
	for (i = 0; i < (int)array_size(touch_zones); i++) {
		int zx1 = touch_zones[i].x1;
		int zy1 = touch_zones[i].y1;
		int zx2 = touch_zones[i].x2;
		int zy2 = touch_zones[i].y2;

		// Check if this zone overlaps with the video area
		int overlaps = !(zx2 <= video_x1_pct || zx1 >= video_x2_pct ||
		                 zy2 <= video_y1_pct || zy1 >= video_y2_pct);

		if (overlaps)
			continue;  // Skip this button - it would be over the video

		// Calculate pixel coordinates
		int x1 = (zx1 * screen_w) / 100;
		int y1 = (zy1 * screen_h) / 100;
		int x2 = (zx2 * screen_w) / 100;
		int y2 = (zy2 * screen_h) / 100;
		int w = x2 - x1;
		int h = y2 - y1;
		int button_id = touch_zones[i].button_id;
		int is_pressed = (buttons_pressed & (1 << button_id)) != 0;
		const char *label = button_labels[button_id];
		int text_x, text_y;

		// Draw outline only
		draw_rect_outline(screen, x1, y1, w, h,
			is_pressed ? color_pressed : color_outline,
			is_pressed ? thickness + 1 : thickness);

		// Draw text label centered in the button
		if (label && label[0] && screen->format->BitsPerPixel == 16) {
			int label_len = strlen(label);
			text_x = x1 + (w - label_len * 8) / 2;
			text_y = y1 + (h - 8) / 2;
			if (text_x >= 0 && text_y >= 0) {
				basic_text_out16_nf(screen->pixels, screen->pitch / 2, text_x, text_y, label);
			}
		}
	}

	if (SDL_MUSTLOCK(screen)) {
		SDL_UnlockSurface(screen);
	}
}

// Get current button state (for GL overlay rendering)
unsigned int in_webos_touch_get_buttons(void)
{
	return buttons_pressed;
}

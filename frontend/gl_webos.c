/*
 * OpenGL ES 2.0 rendering for webOS TouchPad
 * Uses DIRECT EGL initialization - bypasses SDL's GL entirely
 */

#ifdef WEBOS_TOUCHPAD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include <PDL.h>
#include <GLES2/gl2.h>

/* Pure SDL mode - no direct EGL (causes GPU memory conflicts) */

static int gl_initialized = 0;
static GLuint texture_id = 0;
static GLuint program = 0;
static GLuint color_program = 0;  /* For drawing touch controls */
static GLuint vbo = 0;
static GLuint lines_vbo = 0;      /* For touch control lines */
static int tex_w, tex_h;
static int current_w, current_h;

/* TouchPad screen dimensions - fixed at 1024x768 */
#define SCREEN_W 1024
#define SCREEN_H 768

/* Cached uniform/attribute locations to avoid per-frame lookups */
static GLint tex_uniform_loc = -1;
static GLint color_uniform_loc = -1;

/* Shader sources for textured rendering */
static const char *vertex_shader_src =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

static const char *fragment_shader_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_texture, v_texcoord);\n"
    "}\n";

/* Shader sources for solid color rendering (touch controls) */
static const char *color_vertex_shader_src =
    "attribute vec2 a_position;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static const char *color_fragment_shader_src =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "    gl_FragColor = u_color;\n"
    "}\n";

/* Vertex data: position (x,y) + texcoord (u,v) */
static float vertices[] = {
    /* pos x,y    tex u,v */
    -1.0f,  1.0f, 0.0f, 0.0f,  /* top-left */
     1.0f,  1.0f, 1.0f, 0.0f,  /* top-right */
    -1.0f, -1.0f, 0.0f, 1.0f,  /* bottom-left */
     1.0f, -1.0f, 1.0f, 1.0f,  /* bottom-right */
};

/* Touch zone definitions (same as in_webos_touch.c) - x1, y1, x2, y2 as percentages */
typedef struct {
    int x1, y1, x2, y2;
    int button_id;
} touch_zone_t;

static const touch_zone_t touch_zones[] = {
    /* D-pad */
    {  3, 28, 12, 42, 0 },  /* UP */
    {  3, 58, 12, 72, 1 },  /* DOWN */
    {  0, 42,  7, 58, 2 },  /* LEFT */
    {  8, 42, 15, 58, 3 },  /* RIGHT */
    /* Face buttons */
    { 88, 28, 97, 42, 4 },  /* TRIANGLE */
    { 93, 42, 100, 58, 5 }, /* CIRCLE */
    { 88, 58, 97, 72, 6 },  /* CROSS */
    { 85, 42, 92, 58, 7 },  /* SQUARE */
    /* Triggers */
    {  0,  0, 20, 12, 8 },  /* L1 */
    { 80,  0, 100, 12, 9 }, /* R1 */
    {  0, 12, 20, 24, 10 }, /* L2 */
    { 80, 12, 100, 24, 11 }, /* R2 */
    /* Start/Select */
    { 35, 88, 48, 100, 12 }, /* START */
    { 52, 88, 65, 100, 13 }, /* SELECT */
};

#define NUM_TOUCH_ZONES (sizeof(touch_zones) / sizeof(touch_zones[0]))

/* External: get pressed buttons from touch input system */
extern unsigned int in_webos_touch_get_buttons(void);

static GLuint compile_shader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    GLint compiled;

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "webOS GL: Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static int create_program(void)
{
    GLuint vs, fs;
    GLint linked;

    /* Create textured program */
    vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    if (!vs) return -1;

    fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    if (!fs) {
        glDeleteShader(vs);
        return -1;
    }

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_texcoord");
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!linked) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "webOS GL: Program link error: %s\n", log);
        glDeleteProgram(program);
        program = 0;
        return -1;
    }

    /* Create color program for touch controls */
    vs = compile_shader(GL_VERTEX_SHADER, color_vertex_shader_src);
    if (!vs) return -1;

    fs = compile_shader(GL_FRAGMENT_SHADER, color_fragment_shader_src);
    if (!fs) {
        glDeleteShader(vs);
        return -1;
    }

    color_program = glCreateProgram();
    glAttachShader(color_program, vs);
    glAttachShader(color_program, fs);
    glBindAttribLocation(color_program, 0, "a_position");
    glLinkProgram(color_program);
    glGetProgramiv(color_program, GL_LINK_STATUS, &linked);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!linked) {
        char log[512];
        glGetProgramInfoLog(color_program, sizeof(log), NULL, log);
        fprintf(stderr, "webOS GL: Color program link error: %s\n", log);
        glDeleteProgram(color_program);
        color_program = 0;
        return -1;
    }

    /* Cache uniform locations to avoid per-frame lookups */
    tex_uniform_loc = glGetUniformLocation(program, "u_texture");
    color_uniform_loc = glGetUniformLocation(color_program, "u_color");

    printf("webOS GL: Shader programs created successfully\n");
    printf("webOS GL: Cached uniform locations: tex=%d, color=%d\n", tex_uniform_loc, color_uniform_loc);
    return 0;
}

/* Draw a rectangle outline using GL_LINES */
static void draw_rect_outline_gl(float x1, float y1, float x2, float y2,
                                  float r, float g, float b, float a)
{
    float line_vertices[] = {
        /* Top line */
        x1, y1, x2, y1,
        /* Right line */
        x2, y1, x2, y2,
        /* Bottom line */
        x2, y2, x1, y2,
        /* Left line */
        x1, y2, x1, y1
    };

    /* Use cached uniform location */
    glUniform4f(color_uniform_loc, r, g, b, a);

    glBindBuffer(GL_ARRAY_BUFFER, lines_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glDrawArrays(GL_LINES, 0, 8);
}

/* Draw all touch control overlays */
static void draw_touch_controls(void)
{
    unsigned int buttons_pressed;
    int i;

    if (!color_program || !lines_vbo)
        return;

    /* Get current button state */
    buttons_pressed = in_webos_touch_get_buttons();

    /* Set up state once for all touch controls */
    glUseProgram(color_program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnableVertexAttribArray(0);
    glLineWidth(2.0f);

    /* Draw each touch zone */
    for (i = 0; i < (int)NUM_TOUCH_ZONES; i++) {
        /* Convert percentage coordinates to normalized device coordinates (-1 to 1) */
        float x1 = (touch_zones[i].x1 / 50.0f) - 1.0f;
        float y1 = 1.0f - (touch_zones[i].y1 / 50.0f);
        float x2 = (touch_zones[i].x2 / 50.0f) - 1.0f;
        float y2 = 1.0f - (touch_zones[i].y2 / 50.0f);

        int button_id = touch_zones[i].button_id;
        int is_pressed = (buttons_pressed & (1 << button_id)) != 0;

        if (is_pressed) {
            /* Yellow when pressed */
            draw_rect_outline_gl(x1, y1, x2, y2, 1.0f, 1.0f, 0.0f, 0.9f);
        } else {
            /* White outline normally */
            draw_rect_outline_gl(x1, y1, x2, y2, 1.0f, 1.0f, 1.0f, 0.6f);
        }
    }

    glDisableVertexAttribArray(0);
    glDisable(GL_BLEND);
}

int gl_webos_init(void)
{
    PDL_Err err;

    if (gl_initialized)
        return 0;

    printf("webOS GL: Initializing pure SDL mode\n");

    /* Let PDL load the GL libraries */
    err = PDL_LoadOGL(PDL_OGL_2_0);
    if (err != PDL_NOERROR) {
        fprintf(stderr, "webOS GL: PDL_LoadOGL failed: %s\n", PDL_GetError());
        return -1;
    }
    printf("webOS GL: PDL_LoadOGL(2.0) succeeded\n");

    gl_initialized = 1;
    return 0;
}

int gl_webos_create(int w, int h)
{
    printf("webOS GL: Creating GL context %dx%d\n", w, h);

    /* Calculate power-of-two texture size */
    for (tex_w = 1; tex_w < w; tex_w *= 2);
    for (tex_h = 1; tex_h < h; tex_h *= 2);

    printf("webOS GL: Texture size: %dx%d (for %dx%d)\n", tex_w, tex_h, w, h);

    /* Create shader programs */
    if (create_program() < 0)
        return -1;

    /* Create texture - use NEAREST for speed and pixel-perfect rendering */
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Allocate texture storage */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_w, tex_h, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);

    /* Create VBOs */
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &lines_vbo);

    /* Set viewport to full screen - always 1024x768 on TouchPad */
    glViewport(0, 0, SCREEN_W, SCREEN_H);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    current_w = w;
    current_h = h;

    printf("webOS GL: GL context created successfully\n");
    printf("webOS GL: GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    printf("webOS GL: GL_VERSION: %s\n", glGetString(GL_VERSION));
    printf("webOS GL: Viewport set to %dx%d\n", SCREEN_W, SCREEN_H);

    /* Pure SDL mode - using SDL_GL_SwapBuffers */
    printf("webOS GL: Using SDL_GL_SwapBuffers\n");

    return 0;
}

void gl_webos_update_vertices(int src_w, int src_h, int dst_w, int dst_h,
                               int layer_x, int layer_y, int layer_w, int layer_h)
{
    float tex_u = (float)src_w / tex_w;
    float tex_v = (float)src_h / tex_h;

    /* Calculate normalized device coordinates for the layer */
    float x1 = (2.0f * layer_x / dst_w) - 1.0f;
    float y1 = 1.0f - (2.0f * layer_y / dst_h);
    float x2 = (2.0f * (layer_x + layer_w) / dst_w) - 1.0f;
    float y2 = 1.0f - (2.0f * (layer_y + layer_h) / dst_h);

    /* Update vertex data */
    vertices[0]  = x1; vertices[1]  = y1; vertices[2]  = 0.0f;   vertices[3]  = 0.0f;
    vertices[4]  = x2; vertices[5]  = y1; vertices[6]  = tex_u;  vertices[7]  = 0.0f;
    vertices[8]  = x1; vertices[9]  = y2; vertices[10] = 0.0f;   vertices[11] = tex_v;
    vertices[12] = x2; vertices[13] = y2; vertices[14] = tex_u;  vertices[15] = tex_v;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
}

/* Frame skip counter for working around slow swap (~100ms per swap on webOS) */
static int swap_skip = 0;
#define SWAP_EVERY_N_FRAMES 2  /* Swap every 2 frames = ~30fps effective */

int gl_webos_flip(const void *fb, int w, int h)
{
    static int last_w = 0, last_h = 0;
    static int frame_count = 0;
    static Uint32 total_tex = 0, total_draw = 0, total_swap = 0;
    Uint32 t0, t1, t2, t3;
    int do_swap;

    if (!program || !texture_id)
        return -1;

    /* Determine if we should swap this frame */
    swap_skip++;
    do_swap = (swap_skip >= SWAP_EVERY_N_FRAMES);
    if (do_swap)
        swap_skip = 0;

    t0 = SDL_GetTicks();

    /* Update texture with new frame data */
    if (fb != NULL) {
        float tex_u = (float)w / tex_w;
        float tex_v = (float)h / tex_h;

        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, fb);

        /* Update vertices if size changed - calculate centered, aspect-preserving position */
        if (w != last_w || h != last_h) {
            /* TouchPad screen is 1024x768, calculate centered 4:3 output */
            float src_aspect = (float)w / h;
            float dst_aspect = (float)SCREEN_W / SCREEN_H;
            float scale_x, scale_y;
            float x1, y1, x2, y2;

            if (src_aspect > dst_aspect) {
                /* Source is wider - fit to width, letterbox top/bottom */
                scale_x = 1.0f;
                scale_y = dst_aspect / src_aspect;
            } else {
                /* Source is taller - fit to height, pillarbox left/right */
                scale_x = src_aspect / dst_aspect;
                scale_y = 1.0f;
            }

            printf("webOS GL: Centering %dx%d on %dx%d, scale=%.2f,%.2f\n",
                   w, h, SCREEN_W, SCREEN_H, scale_x, scale_y);

            /* Centered normalized device coordinates */
            x1 = -scale_x;
            x2 = scale_x;
            y1 = scale_y;
            y2 = -scale_y;

            /* Update vertex positions and texture coordinates */
            vertices[0]  = x1; vertices[1]  = y1; vertices[2]  = 0.0f;   vertices[3]  = 0.0f;
            vertices[4]  = x2; vertices[5]  = y1; vertices[6]  = tex_u;  vertices[7]  = 0.0f;
            vertices[8]  = x1; vertices[9]  = y2; vertices[10] = 0.0f;   vertices[11] = tex_v;
            vertices[12] = x2; vertices[13] = y2; vertices[14] = tex_u;  vertices[15] = tex_v;

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

            last_w = w;
            last_h = h;
        }
        /* Just update texture coordinates if only those changed */
        else if (vertices[6] != tex_u || vertices[11] != tex_v) {
            vertices[6] = vertices[14] = tex_u;
            vertices[11] = vertices[15] = tex_v;
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        }
    }

    t1 = SDL_GetTicks();

    /* Use shader program and set up state (keep these bound, minimize state changes) */
    glUseProgram(program);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    /* Texture is already bound from glTexSubImage2D above */
    glUniform1i(tex_uniform_loc, 0);

    /* Draw game frame */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    /* Skip touch controls for now - testing performance */
    /* draw_touch_controls(); */

    glDisableVertexAttribArray(1);
    /* Removed glFinish - let GPU work async */

    t2 = SDL_GetTicks();

    /* Swap buffers - pure SDL */
    if (do_swap) {
        SDL_GL_SwapBuffers();
    }

    t3 = SDL_GetTicks();

    /* Accumulate timing */
    total_tex += (t1 - t0);
    total_draw += (t2 - t1);
    if (do_swap)
        total_swap += (t3 - t2);

    /* Print stats every 60 frames */
    frame_count++;
    if (frame_count >= 60) {
        int swaps = 60 / SWAP_EVERY_N_FRAMES;
        printf("webOS GL 60f: tex=%ums draw=%ums swap=%ums (%d swaps, avg swap=%.1fms)\n",
               total_tex, total_draw, total_swap, swaps,
               total_swap / (float)swaps);
        total_tex = total_draw = total_swap = 0;
        frame_count = 0;
    }

    return 0;
}

void gl_webos_clear(void)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapBuffers();
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapBuffers();
}

void gl_webos_destroy(void)
{
    if (texture_id) {
        glDeleteTextures(1, &texture_id);
        texture_id = 0;
    }

    if (vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }

    if (lines_vbo) {
        glDeleteBuffers(1, &lines_vbo);
        lines_vbo = 0;
    }

    if (program) {
        glDeleteProgram(program);
        program = 0;
    }

    if (color_program) {
        glDeleteProgram(color_program);
        color_program = 0;
    }

    current_w = current_h = 0;
}

void gl_webos_shutdown(void)
{
    gl_webos_destroy();
    gl_initialized = 0;
}

int gl_webos_is_active(void)
{
    return gl_initialized && program != 0;
}

#endif /* WEBOS_TOUCHPAD */

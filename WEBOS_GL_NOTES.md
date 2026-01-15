# WebOS OpenGL ES Without Touch Flicker

## Problem
When using OpenGL ES on WebOS (HP TouchPad), touching the screen causes visible flicker. This affects all apps that use EGL directly for GL context management.

## Root Cause
WebOS has a **3-layer display system** (background, application, UI), unlike most systems which have 2 layers. When EGL is used directly to create OpenGL contexts, it doesn't properly integrate with WebOS's layer management system. Touch events trigger layer compositing operations that cause visible flicker.

## Solution
Use **SDL's built-in OpenGL support** instead of EGL directly. SDL internally manages the GL context in a way that integrates properly with WebOS's display system.

### What Works (No Flicker)
- `SDL_GL_SetAttribute()` to configure GL parameters
- `SDL_SetVideoMode()` with `SDL_OPENGL` flag to create GL-capable surface
- `SDL_GL_SwapBuffers()` to flip buffers
- Link directly against `libGLES_CM.so` (OpenGL ES 1.1)

### What Causes Flicker
- Using EGL directly (`eglGetDisplay`, `eglCreateContext`, `eglSwapBuffers`, etc.)
- Loading EGL/GLES libraries via `dlopen()` at runtime

## Implementation

### 1. SDL Video Mode Setup
```c
SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

Uint32 flags = SDL_OPENGL | SDL_FULLSCREEN;
SDL_Surface *screen = SDL_SetVideoMode(width, height, 16, flags);
```

### 2. Buffer Swap
```c
// Use SDL's swap, NOT eglSwapBuffers
SDL_GL_SwapBuffers();
```

### 3. Linking
Link directly against the GLES library:
```makefile
LDLIBS += -lGLES_CM
```

Do NOT link against libEGL.

### 4. GL Function Calls
Use OpenGL ES 1.1 functions directly (they're available from libGLES_CM.so):
```c
#include <GLES/gl.h>

glEnable(GL_TEXTURE_2D);
glGenTextures(1, &texture);
glBindTexture(GL_TEXTURE_2D, texture);
// ... etc
```

## Reference Implementation
See `frontend/libpicofe/gl_webos.c` for a complete WebOS-specific GL implementation.

## Working Examples
- **TuxRacer** (`com.studio3d.tuxr`) - Uses SDL OpenGL, links to libGLES_CM.so, no flicker
- **PCSX-ReARMed** (this project) - After applying this fix

## PDL Initialization
PDL should still be initialized before SDL for proper WebOS integration:
```c
#include <PDL.h>

PDL_Init(0);
// Optional: PDL_SetTouchAggression(PDL_AGGRESSION_MORETOUCHES);

SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
```

## Key Takeaways
1. **Never use EGL directly on WebOS** - it doesn't integrate with the 3-layer system
2. **Let SDL manage the GL context** - SDL knows how to work with WebOS
3. **Link directly to libGLES_CM.so** - don't dlopen it at runtime
4. **PDL_Init before SDL_Init** - ensures proper system integration

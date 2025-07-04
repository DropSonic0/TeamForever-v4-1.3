#ifndef PS3_PPU_PRX_LOADER // If not defined by compiler, define it here for this file
#define PS3_PPU_PRX_LOADER
#endif

#include "RetroEngine.hpp"
#include <cmath> // Para floorf, sinf, cosf, roundf (sinf, cosf, M_PI podrían ser necesarios para DrawSpriteAllFX completo)
#include <stdio.h> // Para printf en debug
#include <string.h> // Para memcpy/memset

// --- Math helper functions ---
#ifndef RETRO_ROUNDF_DEFINED
#define RETRO_ROUNDF_DEFINED
inline float retro_roundf(float x) {
    return floorf(x + 0.5f);
}
#endif

#ifndef RETRO_FMINF_DEFINED
#define RETRO_FMINF_DEFINED
inline float retro_fminf(float a, float b) {
    return (a < b) ? a : b;
}
#endif
// --- FIN DE FUNCIONES DEFINIDAS MANUALMENTE ---

ushort blendLookupTable[0x20 * 0x100];
ushort subtractLookupTable[0x20 * 0x100];
ushort tintLookupTable[0x10000];

#define maxVal(a, b) (a >= b ? a : b)
#define minVal(a, b) (a <= b ? a : b)

// int CURRENT_DISP_SCREEN = 0; // Si no está en Engine struct, podría declararse aquí.

int SCREEN_XSIZE_CONFIG = DEFAULT_SCREEN_XSIZE; // Asumiendo DEFAULT_SCREEN_XSIZE está definido
int SCREEN_XSIZE        = DEFAULT_SCREEN_XSIZE;
int SCREEN_CENTERX      = DEFAULT_SCREEN_XSIZE / 2;

float SCREEN_XSIZE_F   = (float)DEFAULT_SCREEN_XSIZE;
float SCREEN_CENTERX_F = (float)DEFAULT_SCREEN_XSIZE / 2.0f;
float SCREEN_YSIZE_F   = (float)SCREEN_YSIZE; // SCREEN_YSIZE es un define
float SCREEN_CENTERY_F = (float)SCREEN_YSIZE / 2.0f;

int touchWidth     = DEFAULT_SCREEN_XSIZE;
int touchHeight    = SCREEN_YSIZE;
float touchWidthF  = (float)DEFAULT_SCREEN_XSIZE;
float touchHeightF = (float)SCREEN_YSIZE;

DrawListEntry drawListEntries[DRAWLAYER_COUNT];

int gfxDataPosition = 0;
GFXSurface gfxSurface[SURFACE_COUNT]; // Definición del array
byte graphicData[GFXDATA_SIZE];       // Definición del array

DisplaySettings displaySettings;      // Se inicializará por defecto o en InitRenderDevice
bool convertTo32Bit     = false;
bool mixFiltersOnJekyll = false; // << ESTA ES LA DEFINICIÓN QUE FALTABA

#if RETRO_USING_OPENGL 
GLint defaultFramebuffer = -1;
GLuint framebufferHiRes  = -1;
GLuint renderbufferHiRes = -1;
GLuint videoBuffer = -1; 
#endif

#if !RETRO_USE_ORIGINAL_CODE 
bool integerScaling = false;
bool disableEnhancedScaling = false;
bool bilinearScaling = false;
#endif

// --- Defines (Idealmente en un .hpp) ---
#ifndef LAYER_DISABLED
#define LAYER_DISABLED (0xFF) 
#endif

#ifndef FX_NONE
#define FX_NONE (0)
#endif
#ifndef FX_INK
#define FX_INK (1 << 0)
#endif
#ifndef FX_ALPHA
#define FX_ALPHA (1 << 1)
#endif
#ifndef FX_HSCALE
#define FX_HSCALE (1 << 2)
#endif
#ifndef FX_VSCALE
#define FX_VSCALE (1 << 3)
#endif
#ifndef FX_ROTATE
#define FX_ROTATE (1 << 4)
#endif
#ifndef FX_FLIPX
#define FX_FLIPX (1 << 5)
#endif
#ifndef FX_FLIPY
#define FX_FLIPY (1 << 6) 
#endif
#ifndef FX_ALL 
#define FX_ALL (FX_INK | FX_ALPHA | FX_HSCALE | FX_VSCALE | FX_ROTATE | FX_FLIPX | FX_FLIPY)
#endif

int InitRenderDevice()
{
    printf("PS3 DEBUG: InitRenderDevice() started. (Prueba 3.0.2 - Fix GFX_LINESIZE)\n");

    Engine.windowScale = 2; 
    printf("PS3 DEBUG: InitRenderDevice - Engine.windowScale ESTABLECIDO A: %d\n", Engine.windowScale);

    char gameTitle[0x100];
    if (Engine.gameWindowText[0] == '\0') {
        sprintf(gameTitle, "%s (Prueba 3.0.2)", "RSDKv4 PS3");
    } else {
        sprintf(gameTitle, "%s (Prueba 3.0.2)", Engine.gameWindowText);
    }
    printf("PS3 DEBUG: InitRenderDevice - gameTitle: '%s'\n", gameTitle);

#if RETRO_USING_SDL2
    printf("PS3 DEBUG: InitRenderDevice - Dentro de RETRO_USING_SDL2.\n");

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest"); 
    printf("PS3 DEBUG: InitRenderDevice - SDL_HINT_RENDER_SCALE_QUALITY set to 'nearest'.\n");

    Uint32 windowFlags = 0; 

    int window_w = SCREEN_XSIZE * Engine.windowScale; // 852
    int window_h = SCREEN_YSIZE * Engine.windowScale; // 480

    printf("PS3 DEBUG: InitRenderDevice - CALCULATED window_w: %d, window_h: %d PARA CreateWindow\n", window_w, window_h);
    printf("PS3 DEBUG: InitRenderDevice - Attempting SDL_CreateWindow ('%s', %dx%d).\n", gameTitle, window_w, window_h);

    Engine.window = SDL_CreateWindow(gameTitle,
                                     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     window_w, window_h, 
                                     windowFlags);

    if (!Engine.window) {
        printf("PS3 FATAL ERROR: InitRenderDevice - SDL_CreateWindow FAILED! SDL Error: %s\n", SDL_GetError());
        return 0;
    }
    printf("PS3 DEBUG: InitRenderDevice - SDL_CreateWindow() SUCCESSFUL.\n");

    #if !RETRO_USING_OPENGL
        printf("PS3 DEBUG: InitRenderDevice - Attempting SDL_CreateRenderer().\n");
        Engine.renderer = SDL_CreateRenderer(Engine.window, -1, SDL_RENDERER_ACCELERATED); 

        if (!Engine.renderer) {
            printf("PS3 FATAL ERROR: InitRenderDevice - SDL_CreateRenderer FAILED! SDL Error: %s\n", SDL_GetError());
            SDL_DestroyWindow(Engine.window); Engine.window = NULL;
            return 0;
        }
        printf("PS3 DEBUG: InitRenderDevice - SDL_CreateRenderer() successful.\n");

        printf("PS3 DEBUG: InitRenderDevice - SDL_RenderSetLogicalSize SKIPPED for Prueba 3.0.2.\n");

        if (SDL_SetRenderDrawBlendMode(Engine.renderer, SDL_BLENDMODE_BLEND) != 0) {
             printf("PS3 WARNING: InitRenderDevice - SDL_SetRenderDrawBlendMode FAILED: %s\n", SDL_GetError());
        } else {
             printf("PS3 DEBUG: InitRenderDevice - Renderer Blend Mode Set to SDL_BLENDMODE_BLEND.\n");
        }

        #if RETRO_SOFTWARE_RENDER
            printf("PS3 DEBUG: InitRenderDevice - Allocating SDL Textures for software rendering (Prueba 3.0.2).\n");
            if (Engine.gameRenderTexture) {
                SDL_DestroyTexture(Engine.gameRenderTexture);
                Engine.gameRenderTexture = NULL;
                printf("PS3 DEBUG: InitRenderDevice - Old Engine.gameRenderTexture destroyed.\n");
            }

            Engine.gameRenderTexture = SDL_CreateTexture(Engine.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_XSIZE, SCREEN_YSIZE);
            if (!Engine.gameRenderTexture) {
                printf("PS3 FATAL ERROR: InitRenderDevice - SDL_CreateTexture for gameRenderTexture (ARGB8888 STREAMING %dx%d) FAILED! SDL Error: %s\n", SCREEN_XSIZE, SCREEN_YSIZE, SDL_GetError());
                SDL_DestroyRenderer(Engine.renderer); Engine.renderer = NULL;
                SDL_DestroyWindow(Engine.window); Engine.window = NULL;
                return 0; 
            }
            printf("PS3 DEBUG: InitRenderDevice - Engine.gameRenderTexture (ARGB8888 STREAMING, %dx%d) Created.\n", SCREEN_XSIZE, SCREEN_YSIZE);

            if(SDL_SetTextureBlendMode(Engine.gameRenderTexture, SDL_BLENDMODE_BLEND) != 0) {
                printf("PS3 WARNING: InitRenderDevice - SDL_SetTextureBlendMode for gameRenderTexture FAILED: %s\n", SDL_GetError());
            } else {
                printf("PS3 DEBUG: InitRenderDevice - Engine.gameRenderTexture Blend Mode set to SDL_BLENDMODE_BLEND.\n");
            }

            if (Engine.videoTexture) {
                SDL_DestroyTexture(Engine.videoTexture);
                Engine.videoTexture = NULL;
                 printf("PS3 DEBUG: InitRenderDevice - Old Engine.videoTexture destroyed.\n");
            }
            printf("PS3 DEBUG: InitRenderDevice - Engine.videoTexture remains NULL for this test.\n");
        #else
             printf("PS3 DEBUG: InitRenderDevice - RETRO_SOFTWARE_RENDER is FALSE.\n");
        #endif 
    #else
        printf("PS3 DEBUG: InitRenderDevice - RETRO_USING_OPENGL is TRUE.\n");
    #endif 

    Engine.screenRefreshRate = 60;
    SDL_DisplayMode mode;
    if (SDL_GetWindowDisplayMode(Engine.window, &mode) == 0) {
        if (mode.refresh_rate > 0) {
            Engine.screenRefreshRate = mode.refresh_rate;
        }
    }
    printf("PS3 DEBUG: InitRenderDevice - Screen Refresh Rate set to %d Hz.\n", Engine.screenRefreshRate);

#else 
    printf("PS3 FATAL ERROR: InitRenderDevice - RETRO_USING_SDL2 is not defined!\n");
    return 0;
#endif 

    // *** CRITICAL FIX: Set GFX_LINESIZE before allocating Engine.frameBuffer ***
    printf("PS3 DEBUG: InitRenderDevice - Calling SetScreenSize(%d, %d) to set GFX_LINESIZE.\n", SCREEN_XSIZE, SCREEN_XSIZE);
    SetScreenSize(SCREEN_XSIZE, SCREEN_XSIZE); // Sets GFX_LINESIZE = SCREEN_XSIZE (or a padded version if SetScreenSize does that)
    printf("PS3 DEBUG: InitRenderDevice - GFX_LINESIZE is now %d (should be >= %d).\n", GFX_LINESIZE, SCREEN_XSIZE);

    printf("PS3 DEBUG: InitRenderDevice - Internal game resolution: SCREEN_XSIZE=%d, SCREEN_YSIZE=%d.\n", SCREEN_XSIZE, SCREEN_YSIZE);

#if RETRO_SOFTWARE_RENDER
    printf("PS3 DEBUG: InitRenderDevice - Allocating RSDK software framebuffers using GFX_LINESIZE = %d.\n", GFX_LINESIZE);
    if (Engine.frameBuffer) delete[] Engine.frameBuffer;
    Engine.frameBuffer   = new ushort[GFX_LINESIZE * SCREEN_YSIZE]; 
    memset(Engine.frameBuffer, 0, (GFX_LINESIZE * SCREEN_YSIZE) * sizeof(ushort));
    printf("PS3 DEBUG: InitRenderDevice - Software Engine.frameBuffer (RGB565, using GFX_LINESIZE %d x %d) Allocated.\n", GFX_LINESIZE, SCREEN_YSIZE);

    if (Engine.texBuffer) delete[] Engine.texBuffer;
    Engine.texBuffer = new uint[GFX_LINESIZE * SCREEN_YSIZE]; 
    memset(Engine.texBuffer, 0, (GFX_LINESIZE * SCREEN_YSIZE) * sizeof(uint)); 
    printf("PS3 DEBUG: InitRenderDevice - Software Engine.texBuffer (32-bit, using GFX_LINESIZE %d x %d) Allocated.\n", GFX_LINESIZE, SCREEN_YSIZE);
#endif 

    OBJECT_BORDER_X2 = SCREEN_XSIZE + 0x80;
    OBJECT_BORDER_X4 = SCREEN_XSIZE + 0x20;
    printf("PS3 DEBUG: InitRenderDevice - Object borders calculated.\n");

    printf("PS3 DEBUG: InitRenderDevice - Calling InitInputDevices().\n");
    InitInputDevices(); 

    printf("PS3 DEBUG: InitRenderDevice() finished successfully (Prueba 3.0.2 - Fix GFX_LINESIZE).\n");
    return 1;
}

void FlipScreen()
{
#if RETRO_SOFTWARE_RENDER && !RETRO_USING_OPENGL && RETRO_USING_SDL2

    static bool initial_flip_logged_3_3 = false;
    static int frame_count_log_interval = 60; 
    static int current_frame_count_3_3 = 0;

    if (!Engine.renderer || !Engine.gameRenderTexture || !Engine.frameBuffer) {
        if (current_frame_count_3_3 == 0) {
             printf("PS3 FLIPSCREEN ERROR (Prueba 3.3): Critical component NULL! R=%p, GRT=%p, FB=%p.\n", 
                 Engine.renderer, Engine.gameRenderTexture, Engine.frameBuffer);
        }
        if (Engine.renderer) { 
            SDL_SetRenderDrawColor(Engine.renderer, 255, 0, 0, 255); // RED
            SDL_RenderClear(Engine.renderer);
            SDL_RenderPresent(Engine.renderer);
        }
        current_frame_count_3_3++;
        return;
    }
    
    // YA NO HAY LLAMADAS FORZADAS A ClearScreen() AQUÍ.
    // El motor RSDK es responsable de llenar Engine.frameBuffer.

    if (!initial_flip_logged_3_3) {
        printf("PS3 FLIPSCREEN (Prueba 3.3): Primera llamada. GFX_LINESIZE=%d, SCREEN_XSIZE=%d, SCREEN_YSIZE=%d\n", GFX_LINESIZE, SCREEN_XSIZE, SCREEN_YSIZE);
        initial_flip_logged_3_3 = true;
    }
    
    if (current_frame_count_3_3 % frame_count_log_interval == 0) {
        printf("PS3 FLIPSCREEN (Prueba 3.3) Inspecting Engine.frameBuffer (Frame: %d):\n", current_frame_count_3_3);
        if (GFX_LINESIZE > 0 && SCREEN_YSIZE > 0 && Engine.frameBuffer) {
            printf("  Line 0, Pix 0-4: ");
            for (int i = 0; i < 5 && i < SCREEN_XSIZE && i < GFX_LINESIZE; ++i) printf("0x%04X ", Engine.frameBuffer[0 * GFX_LINESIZE + i]);
            printf("\n");
            int mid_y = SCREEN_YSIZE / 2;
            if (mid_y < SCREEN_YSIZE) { 
                 printf("  Line %d, Pix 0-4: ", mid_y);
                 for (int i = 0; i < 5 && i < SCREEN_XSIZE && i < GFX_LINESIZE; ++i) printf("0x%04X ", Engine.frameBuffer[mid_y * GFX_LINESIZE + i]);
                 printf("\n");
            }
        } else {
            printf("  Engine.frameBuffer not inspectable (GFX_LINESIZE=%d, SCREEN_YSIZE=%d, FB Ptr: %p)\n", GFX_LINESIZE, SCREEN_YSIZE, Engine.frameBuffer);
        }
    }

    void *texturePixels = NULL;
    int texturePitch = 0;
    static bool lock_info_logged_3_3 = false; 

    if (SDL_LockTexture(Engine.gameRenderTexture, NULL, &texturePixels, &texturePitch) == 0) {
        if (!lock_info_logged_3_3) {
            printf("PS3 FLIPSCREEN (Prueba 3.3): SDL_LockTexture OK. Pitch: %d. Expected: %d.\n", texturePitch, SCREEN_XSIZE * 4);
            lock_info_logged_3_3 = true; 
        }

        for (int y = 0; y < SCREEN_YSIZE; ++y) {
            uint32_t *dstLineARGB8888 = (uint32_t*)((unsigned char*)texturePixels + y * texturePitch);
            ushort *srcLineRGB565 = &Engine.frameBuffer[y * GFX_LINESIZE];
            for (int x = 0; x < SCREEN_XSIZE; ++x) {
                ushort rgb565_pixel = srcLineRGB565[x];
                uint8_t r5 = (rgb565_pixel >> 11) & 0x1F;
                uint8_t g6 = (rgb565_pixel >> 5) & 0x3F;
                uint8_t b5 = rgb565_pixel & 0x1F;
                uint8_t r8 = (r5 * 255 + 15) / 31;
                uint8_t g8 = (g6 * 255 + 31) / 63;
                uint8_t b8 = (b5 * 255 + 15) / 31;
                dstLineARGB8888[x] = (0xFFU << 24) | (r8 << 16) | (g8 << 8) | b8;
            }
        }
        SDL_UnlockTexture(Engine.gameRenderTexture);
    } else {
        if (current_frame_count_3_3 % frame_count_log_interval == 0 || !lock_info_logged_3_3) {
            printf("PS3 FLIPSCREEN ERROR (Prueba 3.3): SDL_LockTexture FAILED: %s Frame: %d\n", SDL_GetError(), current_frame_count_3_3);
            lock_info_logged_3_3 = true; 
        }
        SDL_SetRenderDrawColor(Engine.renderer, 255, 128, 0, 255); 
        SDL_RenderClear(Engine.renderer);
        SDL_RenderPresent(Engine.renderer);
        current_frame_count_3_3++;
        return; 
    }

    if (SDL_SetRenderDrawColor(Engine.renderer, 0, 0, 0, 255) != 0) { /* ... */ }
    if (SDL_RenderClear(Engine.renderer) != 0) { /* ... */ }
    SDL_Rect dstRect = {0, 0, SCREEN_XSIZE, SCREEN_YSIZE}; 
    if (SDL_RenderCopy(Engine.renderer, Engine.gameRenderTexture, NULL, &dstRect) != 0) {
        if (current_frame_count_3_3 % frame_count_log_interval == 0) {
            printf("PS3 FLIPSCREEN ERROR (Prueba 3.3): SDL_RenderCopy FAILED: %s Frame: %d\n", SDL_GetError(), current_frame_count_3_3);
        }
    }
    SDL_RenderPresent(Engine.renderer);
    current_frame_count_3_3++;

#else 
    if(Engine.renderer) {
        SDL_SetRenderDrawColor(Engine.renderer, 0, 255, 255, 255); 
        SDL_RenderClear(Engine.renderer);
        SDL_RenderPresent(Engine.renderer);
    }
#endif 
}

void ReleaseRenderDevice(bool refresh)
{
    printf("PS3 DEBUG: ReleaseRenderDevice(%s) called.\n", refresh ? "true" : "false");

	if (!refresh) {
		// ClearMeshData(); // Probablemente no relevante para PS3 software
		// ClearTextures(false); // Probablemente no relevante para PS3 software
	}

#if !RETRO_USE_ORIGINAL_CODE 
    #if RETRO_SOFTWARE_RENDER 
        if (Engine.frameBuffer) { 
            delete[] Engine.frameBuffer; 
            Engine.frameBuffer = NULL; 
            printf("PS3 DEBUG: ReleaseRenderDevice - Software Engine.frameBuffer released.\n"); 
        }
        if (Engine.texBuffer) { 
            delete[] Engine.texBuffer; 
            Engine.texBuffer = NULL; 
            printf("PS3 DEBUG: ReleaseRenderDevice - Software Engine.texBuffer released.\n"); 
        }
    #endif 

    #if RETRO_USING_SDL2
        #if !RETRO_USING_OPENGL && RETRO_SOFTWARE_RENDER 
            if (Engine.gameRenderTexture) { 
                SDL_DestroyTexture(Engine.gameRenderTexture); 
                Engine.gameRenderTexture = NULL; 
                printf("PS3 DEBUG: ReleaseRenderDevice - Engine.gameRenderTexture released.\n"); 
            }
            if (Engine.videoTexture) { 
                SDL_DestroyTexture(Engine.videoTexture); 
                Engine.videoTexture = NULL; 
                printf("PS3 DEBUG: ReleaseRenderDevice - Engine.videoTexture released.\n"); 
            }
        #endif 

        #if RETRO_USING_OPENGL 
            // if (Engine.glContext) { SDL_GL_DeleteContext(Engine.glContext); Engine.glContext = NULL; }
        #endif

        if (Engine.renderer) { 
            SDL_DestroyRenderer(Engine.renderer); 
            Engine.renderer = NULL; 
            printf("PS3 DEBUG: ReleaseRenderDevice - Renderer released.\n"); 
        }
        if (Engine.window) { 
            SDL_DestroyWindow(Engine.window); 
            Engine.window = NULL; 
            printf("PS3 DEBUG: ReleaseRenderDevice - Window released.\n"); 
        }
    #endif 
#endif 
    printf("PS3 DEBUG: ReleaseRenderDevice() finished.\n");
}

void GenerateBlendLookupTable(void)
{
    for (int intensity_alpha = 0; intensity_alpha < 0x100; intensity_alpha++) { 
        for (int color_component_5bit = 0; color_component_5bit < 0x20; color_component_5bit++) {
            blendLookupTable[color_component_5bit + (0x20 * intensity_alpha)] = (intensity_alpha * color_component_5bit) >> 8;
            subtractLookupTable[color_component_5bit + (0x20 * intensity_alpha)] = (intensity_alpha * (0x1F - color_component_5bit)) >> 8;
        }
    }

    for (int i = 0; i < 0x10000; i++) { 
        int r_5bit = (i & 0xF800) >> 11;
        int g_6bit = (i & 0x07E0) >> 5;  
        int b_5bit = (i & 0x001F);       
        int average_intensity_5bit = (r_5bit + (g_6bit >> 1) + b_5bit) / 3;
        int tintValue_5bit = minVal(average_intensity_5bit + 6, 0x1F);
        
        ushort final_tinted_color = (ushort)((tintValue_5bit << 11) |  
                                           ((tintValue_5bit << 1) << 5) |  
                                           (tintValue_5bit));          
        tintLookupTable[i] = final_tinted_color; 
    }
}

void ClearScreen(byte paletteIndex) 
{
    #ifdef PS3_PPU_PRX_LOADER
    // INICIO DE MODIFICACIÓN PARA PRUEBA 3.5
    if (activePalette) { 
        printf("PS3_DEBUG: ClearScreen(%u) called. activePalette[paletteIndex] = 0x%04X. GFX_LINESIZE: %d\n", 
               paletteIndex, activePalette[paletteIndex], GFX_LINESIZE);
    } else {
        printf("PS3_DEBUG: ClearScreen(%u) called, but activePalette is NULL! Defaulting fill to 0x0000. GFX_LINESIZE: %d\n", 
               paletteIndex, GFX_LINESIZE);
    }
    // FIN DE MODIFICACIÓN PARA PRUEBA 3.5
    #endif

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) {
        #ifdef PS3_PPU_PRX_LOADER
        printf("PS3_DEBUG: ClearScreen - Engine.frameBuffer is NULL! Cannot clear.\n");
        #endif
        return;
    }

    ushort color_fill = activePalette ? activePalette[paletteIndex] : 0x0000; 
    ushort *fb_ptr    = Engine.frameBuffer;         
    
    if (GFX_LINESIZE <= 0 || SCREEN_YSIZE <= 0) {
         #ifdef PS3_PPU_PRX_LOADER
         printf("PS3_DEBUG: ClearScreen - Invalid dimensions for clearing (GFX_LINESIZE=%d, SCREEN_YSIZE=%d)\n", GFX_LINESIZE, SCREEN_YSIZE);
         #endif
         return;
    }
    int total_pixels = GFX_LINESIZE * SCREEN_YSIZE; 

    for (int i = 0; i < total_pixels; ++i) {
        fb_ptr[i] = color_fill;
    }
#endif
}

void SetScreenDimensions(int window_width_param, int window_height_param) 
{
    printf("PS3 DEBUG: SetScreenDimensions called with window_width: %d, window_height: %d.\n", window_width_param, window_height_param);

    touchWidth    = window_width_param;
    touchHeight   = window_height_param;
    touchWidthF   = (float)window_width_param;
    touchHeightF  = (float)window_height_param;

    displaySettings.width    = window_width_param;
    displaySettings.height   = window_height_param;

    Engine.useHighResAssets = false; 
    printf("PS3 DEBUG: SetScreenDimensions - Engine.useHighResAssets set to %s.\n", Engine.useHighResAssets ? "true" : "false");

    int internal_game_width = SCREEN_XSIZE; 
    int linesize_alignment = (internal_game_width + 7) & ~7; 
                                                          
    SetScreenSize(internal_game_width, linesize_alignment);
    printf("PS3 DEBUG: SetScreenDimensions - Called SetScreenSize with width: %d, linesize: %d.\n", internal_game_width, linesize_alignment);

    // La lógica restante para textureList[0], retroVertexList, screenBufferVertexList es para OpenGL
    // y se puede omitir o comentar para el renderizado por software puro en PS3.

    printf("PS3 DEBUG: SetScreenDimensions - Finished (omitiendo lógica GL de vértices/texturas).\n"
		);
}

void SetScreenSize(int width_param, int lineSize_ushorts_param) 
{
    SCREEN_XSIZE        = width_param;
    SCREEN_CENTERX      = width_param / 2;

    SCREEN_SCROLL_LEFT  = SCREEN_CENTERX - 8; 
    SCREEN_SCROLL_RIGHT = SCREEN_CENTERX + 8;

    OBJECT_BORDER_X2    = width_param + 0x80; 
    OBJECT_BORDER_X4    = width_param + 0x20; 

    GFX_LINESIZE          = lineSize_ushorts_param;
    GFX_LINESIZE_MINUSONE = lineSize_ushorts_param - 1;
    GFX_LINESIZE_DOUBLE   = 2 * lineSize_ushorts_param; 

    GFX_FRAMEBUFFERSIZE   = SCREEN_YSIZE * lineSize_ushorts_param; 
    GFX_FBUFFERMINUSONE   = GFX_FRAMEBUFFERSIZE - 1;

    // printf("PS3 DEBUG: SetScreenSize - SCREEN_XSIZE=%d, GFX_LINESIZE=%d\n", SCREEN_XSIZE, GFX_LINESIZE);
}

#if RETRO_SOFTWARE_RENDER
void CopyFrameOverlay2x()
{
    if (!Engine.frameBuffer || !Engine.frameBuffer2x) { 
        // printf("PS3 DEBUG CopyFrameOverlay2x: Skipping, buffer(s) NULL.\n");
        return; 
    }

    ushort *src_fb_line_start = &Engine.frameBuffer[((SCREEN_YSIZE / 2) + 12) * GFX_LINESIZE];
    ushort *dst_fb_hq_line_start = Engine.frameBuffer2x;

    for (int y = 0; y < (SCREEN_YSIZE / 2) - 12; ++y) {
        ushort *current_src_pixel = src_fb_line_start;
        ushort *current_dst_pixel_pass1 = dst_fb_hq_line_start;
        ushort *current_dst_pixel_pass2 = dst_fb_hq_line_start + GFX_LINESIZE_DOUBLE; // Segunda línea en HQ buffer

        for (int x = 0; x < GFX_LINESIZE; ++x) {
            ushort pixel_val = *current_src_pixel;
            if (pixel_val == 0xF81F) { // magenta
                current_dst_pixel_pass1 += 2;
                current_dst_pixel_pass2 += 2;
            }
            else {
                current_dst_pixel_pass1[0] = pixel_val;
                current_dst_pixel_pass1[1] = pixel_val;
                current_dst_pixel_pass1 += 2;

                current_dst_pixel_pass2[0] = pixel_val;
                current_dst_pixel_pass2[1] = pixel_val;
                current_dst_pixel_pass2 += 2;
            }
            current_src_pixel++;
        }
        src_fb_line_start += GFX_LINESIZE; // Avanzar a la siguiente línea en el buffer fuente
        dst_fb_hq_line_start += GFX_LINESIZE_DOUBLE * 2; // Avanzar dos líneas en el buffer HQ de destino
    }
}
#endif

void SetupViewport()
{
    // Para el renderizado por software en PS3, esta función tiene un propósito muy limitado.
    // La configuración crítica de SCREEN_XSIZE, GFX_LINESIZE ya se hace en InitRenderDevice -> SetScreenDimensions -> SetScreenSize.
    
    // Forzamos useHighResAssets a false para PS3 para simplificar.
    Engine.useHighResAssets = false;

    // El resto de la lógica original de esta función es para OpenGL y puede ser omitida.
    // printf("PS3 DEBUG: SetupViewport() called - Lógica GL omitida..\n");
}

void SetFullScreen(bool fullscreen_active) 
{
    if (!Engine.window) {
        return;
    }

    #if RETRO_USING_SDL2 
        if (fullscreen_active) {
            if (SDL_SetWindowFullscreen(Engine.window, SDL_WINDOW_FULLSCREEN_DESKTOP) < 0) {
                 printf("PS3 WARNING: SetFullScreen(true) - SDL_SetWindowFullscreen FAILED: %s\n", SDL_GetError());
            } else {
                 printf("PS3 DEBUG: SetFullScreen(true) - SDL_SetWindowFullscreen(SDL_WINDOW_FULLSCREEN_DESKTOP) llamado.\n");
            }
            SDL_ShowCursor(SDL_FALSE); 
        } else {
            if (SDL_SetWindowFullscreen(Engine.window, 0) < 0) { 
                 printf("PS3 WARNING: SetFullScreen(false) - SDL_SetWindowFullscreen FAILED: %s\n", SDL_GetError());
            } else {
                printf("PS3 DEBUG: SetFullScreen(false) - SDL_SetWindowFullscreen(0) llamado.\n");
                // Opcional: Restaurar tamaño de ventana si se sale de fullscreen
                // SDL_SetWindowSize(Engine.window, SCREEN_XSIZE_CONFIG * Engine.windowScale, SCREEN_YSIZE * Engine.windowScale);
            }
            SDL_ShowCursor(SDL_TRUE); 
        }
        // La lógica de OpenGL para reajustar displaySettings y llamar a SetupViewport se omite aquí
        // para el renderizado por software, ya que SDL_RenderSetLogicalSize maneja el escalado.
    #elif RETRO_USING_SDL1
        // Lógica SDL1 original...
    #endif

    Engine.isFullScreen = fullscreen_active; 
    printf("PS3 DEBUG: SetFullScreen - Engine.isFullScreen ahora es %s.\n", Engine.isFullScreen ? "true" : "false");
}

void DrawObjectList(int layerID) 
{
    // Prueba 3.5: Debug RSDK DrawObjectList
    printf("RSDK_TRACE: DrawObjectList(%d) called. List size: %d\n", layerID, drawListEntries[layerID].listSize);
    // End Prueba 3.5 debug

    if (layerID < 0 || layerID >= DRAWLAYER_COUNT) { // DRAWLAYER_COUNT is from RSDK headers
        return;
    }

    int current_list_size = drawListEntries[layerID].listSize; 

    for (int i = 0; i < current_list_size; ++i) {
        objectEntityPos = drawListEntries[layerID].entityRefs[i];

        if (objectEntityPos < 0 || objectEntityPos >= ENTITY_COUNT) { // ENTITY_COUNT from RSDK
            continue; 
        }

        int object_type = objectEntityList[objectEntityPos].type; 

        if (object_type > 0 && object_type < OBJECT_COUNT) { // OBJECT_COUNT from RSDK
            if (objectScriptList[object_type].eventDraw.scriptCodePtr > 0 &&
                objectScriptList[object_type].eventDraw.scriptCodePtr < SCRIPT_DATA_SIZE) // SCRIPT_DATA_SIZE from RSDK
            {
                ProcessScript(objectScriptList[object_type].eventDraw.scriptCodePtr, 
                              objectScriptList[object_type].eventDraw.jumpTablePtr, 
                              EVENT_DRAW); // EVENT_DRAW from RSDK
            }
        }
    }
}

void DrawStageGFX()
{
    #ifdef PS3_PPU_PRX_LOADER
    printf("PS3_DEBUG: DrawStageGFX() entered.\n");
    #endif
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) {
        return;
    }
#endif

    waterDrawPos = waterLevel - yScrollOffset;
    if (waterDrawPos < 0) waterDrawPos = 0;
    if (waterDrawPos > SCREEN_YSIZE) waterDrawPos = SCREEN_YSIZE;

    // bool currentDrawStageGFXHQ = false; // Variable local para manejar el estado HQ por capa

    if (tLayerMidPoint < 3) {
        DrawObjectList(0);
        if (activeTileLayers[0] < LAYER_COUNT && stageLayouts[activeTileLayers[0]].type != LAYER_DISABLED) {
            // currentDrawStageGFXHQ = (stageLayouts[activeTileLayers[0]].type == LAYER_3DSKY && Engine.useHQModes);
            // bool oldHQ = drawStageGFXHQ; drawStageGFXHQ = currentDrawStageGFXHQ; // Set global for called function
            switch (stageLayouts[activeTileLayers[0]].type) {
                case LAYER_HSCROLL: DrawHLineScrollLayer(0); break;
                case LAYER_VSCROLL: DrawVLineScrollLayer(0); break;
                case LAYER_3DFLOOR: /*drawStageGFXHQ = false;*/ Draw3DFloorLayer(0); break; 
                case LAYER_3DSKY:   /*drawStageGFXHQ = Engine.useHQModes;*/ Draw3DSkyLayer(0); break; 
                default: break;
            }
            // drawStageGFXHQ = oldHQ; // Restore global
        }
        DrawObjectList(1);
        if (activeTileLayers[1] < LAYER_COUNT && stageLayouts[activeTileLayers[1]].type != LAYER_DISABLED) {
            switch (stageLayouts[activeTileLayers[1]].type) { /* ... */ }
        }
        DrawObjectList(2); DrawObjectList(3); DrawObjectList(4);
        if (activeTileLayers[2] < LAYER_COUNT && stageLayouts[activeTileLayers[2]].type != LAYER_DISABLED) {
            switch (stageLayouts[activeTileLayers[2]].type) { /* ... */ }
        }
    } else if (tLayerMidPoint < 6) {
        DrawObjectList(0);
        if (activeTileLayers[0] < LAYER_COUNT && stageLayouts[activeTileLayers[0]].type != LAYER_DISABLED) { /* ... */ }
        DrawObjectList(1);
        if (activeTileLayers[1] < LAYER_COUNT && stageLayouts[activeTileLayers[1]].type != LAYER_DISABLED) { /* ... */ }
        DrawObjectList(2);
        if (activeTileLayers[2] < LAYER_COUNT && stageLayouts[activeTileLayers[2]].type != LAYER_DISABLED) { /* ... */ }
        DrawObjectList(3); DrawObjectList(4);
    }

    if (tLayerMidPoint < 6) {
        if (activeTileLayers[3] < LAYER_COUNT && stageLayouts[activeTileLayers[3]].type != LAYER_DISABLED) {
            switch (stageLayouts[activeTileLayers[3]].type) { /* ... */ }
        }
        DrawObjectList(5);
        DrawObjectList(6);
    }

    // Para PS3, forzar no-HQ para efectos de pantalla completa y debug para simplificar.
    bool final_drawStageGFXHQ_for_effects = false; 
    // Si quieres mantener la lógica original de HQ para efectos, usa:
    // bool final_drawStageGFXHQ_for_effects = drawStageGFXHQ; 

#if !RETRO_USE_ORIGINAL_CODE
    // if (final_drawStageGFXHQ_for_effects) DrawDebugOverlays(); // Dibuja sobre buffer HQ
#endif

#if RETRO_SOFTWARE_RENDER
    if (final_drawStageGFXHQ_for_effects) {
        if (Engine.frameBuffer2x) { 
            CopyFrameOverlay2x();
    		switch (fadeMode) {
    			case 1:
    			    if (fadeA > 0xFF) fadeA = 0xFF;
                    // DrawRectangle(0, 0, SCREEN_XSIZE, SCREEN_YSIZE, fadeR, fadeG, fadeB, fadeA); // Originalmente aquí
                    SetFadeHQ(fadeR, fadeG, fadeB, fadeA); // Debería afectar el buffer HQ
    			    break;
    			case 2:
    			    // DrawClassicFade(0, 0, SCREEN_XSIZE, SCREEN_YSIZE, fadeR, fadeG, fadeB, fadeX); // Originalmente aquí
                    // Necesitaría una versión HQ de DrawClassicFade que opere en Engine.frameBuffer2x
    			    break;
            }
        } else { // Fallback si el buffer HQ no existe pero se esperaba
            switch (fadeMode) { /* ... Lógica de fade para Engine.frameBuffer ... */ }
        }
    } else {
		switch (fadeMode) {
			case 1:
			    if (fadeA > 0xFF) fadeA = 0xFF;
                DrawRectangle(0, 0, SCREEN_XSIZE, SCREEN_YSIZE, fadeR, fadeG, fadeB, fadeA);
			    break;
			case 2:
			    DrawClassicFade(0, 0, SCREEN_XSIZE, SCREEN_YSIZE, fadeR, fadeG, fadeB, fadeX);
			    break;
        }
    }
#endif

#if !RETRO_USE_ORIGINAL_CODE
    // if (!final_drawStageGFXHQ_for_effects) DrawDebugOverlays(); // Dibuja sobre buffer normal
#endif
}

#if !RETRO_USE_ORIGINAL_CODE 
void DrawDebugOverlays()
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) {
        return;
    }

    if (showHitboxes && debugHitboxList) { 
        for (int i = 0; i < debugHitboxCount; ++i) {
            if (i >= MAX_DEBUG_HITBOXES) break; 

            DebugHitboxInfo *info = &debugHitboxList[i];
            
            int world_x = info->xpos + (info->left << 16);
            int world_y = info->ypos + (info->top << 16);
            int box_w   = abs((info->xpos + (info->right << 16)) - world_x) >> 16;
            int box_h   = abs((info->ypos + (info->bottom << 16)) - world_y) >> 16;
            
            int screen_x = (world_x >> 16) - xScrollOffset;
            int screen_y = (world_y >> 16) - yScrollOffset;

            switch (info->type) {
                case H_TYPE_TOUCH: 
                    if (showHitboxes & 1) 
                        DrawRectangle(screen_x, screen_y, box_w, box_h, info->collision ? 0x80 : 0xFF, info->collision ? 0x80 : 0x00, 0x00, 0x60);
                    break;
                case H_TYPE_BOX: 
                    if (showHitboxes & 1) {
                        DrawRectangle(screen_x, screen_y, box_w, box_h, 0x00, 0x00, 0xFF, 0x60); 
                        if (info->collision & 1) DrawRectangle(screen_x, screen_y, box_w, 1, 0xFF, 0xFF, 0x00, 0xC0); 
                        if (info->collision & 8) DrawRectangle(screen_x, screen_y + box_h -1, box_w, 1, 0xFF, 0xFF, 0x00, 0xC0); 
                        if (info->collision & 2) { 
                            int sy = screen_y; int sh = box_h; if (info->collision & 1) { sy++; sh--; } if (info->collision & 8) { sh--; }
                            DrawRectangle(screen_x, sy, 1, sh, 0xFF, 0xFF, 0x00, 0xC0);
                        }
                        if (info->collision & 4) { 
                            int sy = screen_y; int sh = box_h; if (info->collision & 1) { sy++; sh--; } if (info->collision & 8) { sh--; }
                            DrawRectangle(screen_x + box_w -1, sy, 1, sh, 0xFF, 0xFF, 0x00, 0xC0);
                        }
                    }
                    break;
                case H_TYPE_PLAT: 
                    if (showHitboxes & 1) {
                        DrawRectangle(screen_x, screen_y, box_w, box_h, 0x00, 0xFF, 0x00, 0x60); 
                        if (info->collision & 1) DrawRectangle(screen_x, screen_y, box_w, 1, 0xFF, 0xFF, 0x00, 0xC0); 
                    }
                    break;
                case H_TYPE_FINGER: 
                    if (showHitboxes & 2) 
                        DrawRectangle(screen_x + xScrollOffset, screen_y + yScrollOffset, box_w, box_h, 0xF0, 0x00, 0xF0, 0x60); 
                    break;
                default: break; 
            }
        }
    }

    if (Engine.showPaletteOverlay && fullPalette32) { 
        // Ajustar estas bases para la posición deseada de la paleta en pantalla
        int base_palette_x = SCREEN_XSIZE - (16 * 2 * 2) - 10; // Ej: 2 paletas de 16x2 colores, más margen
        int base_palette_y = SCREEN_YSIZE - (16 * 2) - 10;   // Ej: 16 filas de colores, más margen

        for (int p = 0; p < PALETTE_COUNT; ++p) {
            int p_draw_x = base_palette_x + ((p % (PALETTE_COUNT / 2)) * (16 * 2 + 4));
            int p_draw_y = base_palette_y + ((p / (PALETTE_COUNT / 2)) * (16 * 2 + 4));

            for (int c = 0; c < PALETTE_COLOR_COUNT; ++c) {
                if (p * PALETTE_COLOR_COUNT + c >= MAX_PALETTE_COUNT * PALETTE_COLOR_COUNT) break; 

                int r_comp = fullPalette32[p][c].r;
                int g_comp = fullPalette32[p][c].g;
                int b_comp = fullPalette32[p][c].b;
                unsigned char a_comp = 0xFF;

                if (drawStageGFXHQ && r_comp == 0xFF && g_comp == 0x00 && b_comp == 0xFF) {
                    g_comp = 0x08; 
                }
                int color_disp_x = p_draw_x + ((c % 16) * 2);
                int color_disp_y = p_draw_y + ((c / 16) * 2);
                DrawRectangle(color_disp_x, color_disp_y, 2, 2, r_comp, g_comp, b_comp, a_comp);
            }
        }
    }
#endif 
}
#endif 

void DrawHLineScrollLayer(int layerID_param) 
{
    // Prueba 3.5: Debug RSDK DrawHLineScrollLayer
    printf("RSDK_TRACE: DrawHLineScrollLayer(%d) called.\n", layerID_param);
    // End Prueba 3.5 debug

    if (layerID_param < 0 || layerID_param >= STAGELAYER_COUNT || 
        activeTileLayers[layerID_param] >= LAYER_COUNT) { 
        return;
    }
    TileLayer *layer = &stageLayouts[activeTileLayers[layerID_param]];
    if (!layer->xsize || !layer->ysize || layer->type == LAYER_DISABLED || !layer->tiles || !layer->lineScroll) {
        return;
    }

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) {
        return;
    }

    int screenwidth16_calc = (GFX_LINESIZE >> 4) - 1; 
    int layerwidth_tiles   = layer->xsize;           
    int layerheight_tiles  = layer->ysize;           
    bool layer_is_above_midpoint = (activeTileLayers[layerID_param] >= tLayerMidPoint);

    byte *current_line_scroll_data; 
    int *current_deformation_data;  
    int *current_deformation_data_water; 
    int y_scroll_offset_layer = 0; 

    // --- Lógica BG/FG para scroll y deformación (adaptada de tu original) ---
    // (Esta sección necesita ser precisa según la estructura de tu juego)
    if (layerID_param > 0) { // Asumiendo layerID_param 0 es FG, >0 es BG
        int yScroll_world = yScrollOffset * layer->parallaxFactor >> 8;
        int full_layer_height_pixels = layerheight_tiles << 7;
        layer->scrollPos += layer->scrollSpeed;
        if (layer->scrollPos > (full_layer_height_pixels << 16)) layer->scrollPos -= (full_layer_height_pixels << 16);
        if (layer->scrollPos < 0) layer->scrollPos += (full_layer_height_pixels << 16); // Considerar negativo
        y_scroll_offset_layer = (yScroll_world + (layer->scrollPos >> 16)) % full_layer_height_pixels;
        current_line_scroll_data = layer->lineScroll;
        current_deformation_data = &bgDeformationData2[(byte)(y_scroll_offset_layer + layer->deformationOffset)];
        current_deformation_data_water = &bgDeformationData3[(byte)(y_scroll_offset_layer + waterDrawPos + layer->deformationOffsetW)];
    } else {
        y_scroll_offset_layer = yScrollOffset;
        current_line_scroll_data = layer->lineScroll;
        // for (int i = 0; i < PARALLAX_COUNT; ++i) hParallax.linePos[i] = xScrollOffset; // Asumiendo hParallax global
        current_deformation_data = &bgDeformationData0[(byte)(y_scroll_offset_layer + layer->deformationOffset)];
        current_deformation_data_water = &bgDeformationData1[(byte)(y_scroll_offset_layer + waterDrawPos + layer->deformationOffsetW)];
    }
    // --- Fin Lógica BG/FG ---

    ushort *frame_buffer_write_ptr = Engine.frameBuffer;
    byte *current_line_palette_idx_ptr = gfxLineBuffer;
    int current_tile_y_in_layer_pixels = y_scroll_offset_layer % (layerheight_tiles << 7);
    if (current_tile_y_in_layer_pixels < 0) current_tile_y_in_layer_pixels += (layerheight_tiles << 7);
    
    byte *scroll_index_ptr = &current_line_scroll_data[current_tile_y_in_layer_pixels];
    int current_tile_pixel_y_offset = current_tile_y_in_layer_pixels & 0xF;
    int current_tile_chunk_y        = current_tile_y_in_layer_pixels >> 7;
    int current_tile_y_in_chunk     = (current_tile_y_in_layer_pixels & 0x7F) >> 4;

    int lines_to_draw_segments[2] = { waterDrawPos, SCREEN_YSIZE - waterDrawPos };
    for (int segment = 0; segment < 2; ++segment) {
        int lines_in_segment = lines_to_draw_segments[segment];
        while (lines_in_segment-- > 0) {
            if (current_line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) {
                activePalette   = fullPalette[*current_line_palette_idx_ptr];
                activePalette32 = fullPalette32[*current_line_palette_idx_ptr];
            }
            current_line_palette_idx_ptr++;

            int current_line_scroll_x = 0;
            if (scroll_index_ptr < &current_line_scroll_data[(layerheight_tiles << 7)] && *scroll_index_ptr < PARALLAX_COUNT /*Seguridad para hParallax*/) { 
                 current_line_scroll_x = hParallax.linePos[*scroll_index_ptr];
                if (segment == 0) {
                    if (hParallax.deform[*scroll_index_ptr]) current_line_scroll_x += *current_deformation_data;
                    current_deformation_data++; 
                } else {
                    if (hParallax.deform[*scroll_index_ptr]) current_line_scroll_x += *current_deformation_data_water;
                    current_deformation_data_water++;
                }
                scroll_index_ptr++;
            }

            int full_layer_width_pixels = layerwidth_tiles << 7;
            if (current_line_scroll_x < 0) current_line_scroll_x = (current_line_scroll_x % full_layer_width_pixels) + full_layer_width_pixels;
            current_line_scroll_x %= full_layer_width_pixels;
            
            int current_tile_chunk_x         = current_line_scroll_x >> 7;
            int current_tile_pixel_x_offset  = current_line_scroll_x & 0xF;
            int pixels_remaining_in_first_tile = TILE_SIZE - current_tile_pixel_x_offset;
            int chunk_data_idx = (layer->tiles[current_tile_chunk_x + (current_tile_chunk_y * layerwidth_tiles)] << 6) + 
                                 ((current_line_scroll_x & 0x7F) >> 4) + (8 * current_tile_y_in_chunk);

            int gfx_offset_y_base  = TILE_SIZE * current_tile_pixel_y_offset;
            int gfx_offset_y_flipX = TILE_SIZE * current_tile_pixel_y_offset + (TILE_SIZE - 1);
            int gfx_offset_y_flipY = TILE_SIZE * ((TILE_SIZE - 1) - current_tile_pixel_y_offset);
            int gfx_offset_y_flipXY= TILE_SIZE * ((TILE_SIZE - 1) - current_tile_pixel_y_offset) + (TILE_SIZE - 1);
            
            int pixels_to_write_on_line = GFX_LINESIZE;
            int pixels_from_first_tile = (pixels_remaining_in_first_tile > pixels_to_write_on_line) ? pixels_to_write_on_line : pixels_remaining_in_first_tile;

            if (chunk_data_idx < MAX_TILES_IN_SHEET && tiles128x128.visualPlane[chunk_data_idx] == (byte)layer_is_above_midpoint) { // MAX_TILES_IN_SHEET debe definirse
                byte *gfxDataTilePtr = NULL; 
                int dir = tiles128x128.direction[chunk_data_idx];
                int gfxPos = tiles128x128.gfxDataPos[chunk_data_idx];
                if(gfxPos >= GFXDATA_SIZE) continue; // Comprobación de seguridad crítica

                switch (dir) {
                    case FLIP_NONE: gfxDataTilePtr = &tilesetGFXData[gfxPos + gfx_offset_y_base + current_tile_pixel_x_offset]; break;
                    case FLIP_X:    gfxDataTilePtr = &tilesetGFXData[gfxPos + gfx_offset_y_flipX - current_tile_pixel_x_offset]; break;
                    case FLIP_Y:    gfxDataTilePtr = &tilesetGFXData[gfxPos + gfx_offset_y_flipY + current_tile_pixel_x_offset]; break;
                    case FLIP_XY:   gfxDataTilePtr = &tilesetGFXData[gfxPos + gfx_offset_y_flipXY - current_tile_pixel_x_offset]; break;
                    default: continue;
                }
                for (int px_count = 0; px_count < pixels_from_first_tile; ++px_count) {
                    if (gfxDataTilePtr < &tilesetGFXData[GFXDATA_SIZE] && gfxDataTilePtr >= tilesetGFXData && *gfxDataTilePtr > 0) *frame_buffer_write_ptr = activePalette[*gfxDataTilePtr];
                    frame_buffer_write_ptr++;
                    if (dir == FLIP_X || dir == FLIP_XY) gfxDataTilePtr--; else gfxDataTilePtr++;
                }
            } else {
                frame_buffer_write_ptr += pixels_from_first_tile; 
            }
            pixels_to_write_on_line -= pixels_from_first_tile;

            int current_tile_x_in_chunk = ((current_line_scroll_x & 0x7F) >> 4) + 1;
            for (int tile_iter = 0; tile_iter < screenwidth16_calc && pixels_to_write_on_line >= TILE_SIZE; ++tile_iter) {
                if (current_tile_x_in_chunk >= 8) { 
                    current_tile_x_in_chunk = 0; current_tile_chunk_x++;
                    if (current_tile_chunk_x >= layerwidth_tiles) current_tile_chunk_x = 0; 
                }
                chunk_data_idx = (layer->tiles[current_tile_chunk_x + (current_tile_chunk_y * layerwidth_tiles)] << 6) + 
                                 current_tile_x_in_chunk + (8 * current_tile_y_in_chunk);
                if (chunk_data_idx < MAX_TILES_IN_SHEET && tiles128x128.visualPlane[chunk_data_idx] == (byte)layer_is_above_midpoint) {
                    byte *gfxDataTilePtr = NULL; 
                    int dir = tiles128x128.direction[chunk_data_idx];
                    int gfxPos = tiles128x128.gfxDataPos[chunk_data_idx];
                    if(gfxPos >= GFXDATA_SIZE) continue;

                    switch (dir) { /* ... igual que arriba ... */ }
                    // BUCLE SIMPLIFICADO (reemplazar con desenrollado original para rendimiento)
                    for (int px_idx = 0; px_idx < TILE_SIZE; ++px_idx) {
                         if (gfxDataTilePtr < &tilesetGFXData[GFXDATA_SIZE] && gfxDataTilePtr >= tilesetGFXData && *gfxDataTilePtr > 0) *frame_buffer_write_ptr = activePalette[*gfxDataTilePtr];
                         frame_buffer_write_ptr++;
                         if (dir == FLIP_X || dir == FLIP_XY) gfxDataTilePtr--; else gfxDataTilePtr++;
                    }
                } else {
                    frame_buffer_write_ptr += TILE_SIZE;
                }
                pixels_to_write_on_line -= TILE_SIZE;
                current_tile_x_in_chunk++;
            }
            
            if (pixels_to_write_on_line > 0) { /* ... Dibujar último tile parcial ... */ }

            current_tile_pixel_y_offset++;
            if (current_tile_pixel_y_offset >= TILE_SIZE) {
                current_tile_pixel_y_offset = 0; current_tile_y_in_chunk++;
                if (current_tile_y_in_chunk >= 8) { 
                    current_tile_y_in_chunk = 0; current_tile_chunk_y++;
                    if (current_tile_chunk_y >= layerheight_tiles) { 
                        current_tile_chunk_y = 0;
                         if (scroll_index_ptr >= &current_line_scroll_data[(layerheight_tiles << 7)]) {
                            scroll_index_ptr -= (layerheight_tiles << 7); 
                         }
                    }
                }
            }
        } 
    } 
#endif 
}

void DrawVLineScrollLayer(int layerID_param) 
{
    if (layerID_param < 0 || layerID_param >= STAGELAYER_COUNT || 
        activeTileLayers[layerID_param] >= LAYER_COUNT) { 
        return;
    }
    TileLayer *layer = &stageLayouts[activeTileLayers[layerID_param]];
    if (!layer->xsize || !layer->ysize || layer->type == LAYER_DISABLED || !layer->tiles || !layer->lineScroll) {
        return;
    }

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) {
        return;
    }

    int layerwidth_tiles  = layer->xsize;    
    int layerheight_tiles = layer->ysize;    
    bool layer_is_above_midpoint = (activeTileLayers[layerID_param] >= tLayerMidPoint);

    byte *current_line_scroll_data; 
    int *current_deformation_data;  
    int x_scroll_offset_layer = 0; 

    // --- Lógica BG/FG para scroll y deformación (adaptada de tu original) ---
    // (Esta sección necesita ser precisa según la estructura de tu juego)
    if (layerID_param > 0) { /* ... lógica BG ... */ 
        int xScroll_world = xScrollOffset * layer->parallaxFactor >> 8; 
        int full_layer_width_pixels = layerwidth_tiles << 7;
        layer->scrollPos += layer->scrollSpeed;
        if (layer->scrollPos > (full_layer_width_pixels << 16)) layer->scrollPos -= (full_layer_width_pixels << 16);
        if (layer->scrollPos < 0) layer->scrollPos += (full_layer_width_pixels << 16);
        x_scroll_offset_layer = (xScroll_world + (layer->scrollPos >> 16)) % full_layer_width_pixels;
        current_line_scroll_data = layer->lineScroll;
        current_deformation_data = &bgDeformationData2[(byte)(x_scroll_offset_layer + layer->deformationOffset)];
    } else {
        x_scroll_offset_layer = xScrollOffset; 
        current_line_scroll_data = layer->lineScroll;
        // vParallax.linePos[0] = yScrollOffset; 
        // vParallax.deform[0]  = true;
        current_deformation_data = &bgDeformationData0[(byte)(x_scroll_offset_layer + layer->deformationOffset)];
    }
    // --- Fin Lógica BG/FG ---

    // --- Bucle principal de dibujado de columnas (ESTA ES UNA REPRESENTACIÓN MUY SIMPLIFICADA) ---
    // --- DEBES USAR TU LÓGICA ORIGINAL DETALLADA Y OPTIMIZADA DE RSDKV4 AQUÍ ---
    ushort *frame_buffer_column_start_ptr = Engine.frameBuffer; 
    if (gfxLineBuffer[0] < PALETTE_COUNT) activePalette = fullPalette[gfxLineBuffer[0]];

    int current_tile_x_in_layer_pixels = x_scroll_offset_layer % (layerwidth_tiles << 7);
    if (current_tile_x_in_layer_pixels < 0) current_tile_x_in_layer_pixels += (layerwidth_tiles << 7);
    byte *scroll_index_ptr = &current_line_scroll_data[current_tile_x_in_layer_pixels];
    // ... más inicializaciones de variables para X ...

    for (int screen_x_col = 0; screen_x_col < SCREEN_XSIZE; ++screen_x_col) {
        ushort *frame_buffer_current_pixel_ptr = frame_buffer_column_start_ptr + screen_x_col;
        int current_col_scroll_y = 0;
        if (scroll_index_ptr < &current_line_scroll_data[(layerwidth_tiles << 7)] && *scroll_index_ptr < VPARALLAX_COUNT) { 
            // current_col_scroll_y = vParallax.linePos[*scroll_index_ptr];
            // if (vParallax.deform[*scroll_index_ptr]) current_col_scroll_y += *current_deformation_data;
            current_deformation_data++; 
            scroll_index_ptr++;         
        }
        // ... (Lógica de RSDKv4 para envolver scroll Y, calcular tiles Y, y dibujar la columna de píxeles) ...
        // ... (Esto implica bucles internos para los tiles en la columna, manejo de flips, etc.) ...
        // ... (El código original con desenrollado de bucles es esencial aquí para el rendimiento) ...

        // ... (Avanzar variables de estado para la siguiente columna X) ...
    }
#endif 
}

void Draw3DFloorLayer(int layerID_param) 
{
    if (layerID_param < 0 || layerID_param >= STAGELAYER_COUNT || activeTileLayers[layerID_param] >= LAYER_COUNT) return;
    TileLayer *layer = &stageLayouts[activeTileLayers[layerID_param]];
    if (!layer->xsize || !layer->ysize || layer->type == LAYER_DISABLED || !layer->tiles || !tile3DFloorBuffer) return;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;

    int layer_pixel_width  = layer->xsize << 7; 
    int layer_pixel_height = layer->ysize << 7;
    int layer_y_pos_on_screen = layer->ypos;
    int layer_z_pos_world = layer->zpos;
    int layer_x_pos_world = layer->xpos;

    int angle_idx = layer->angle & (M7_TABLE_SIZE - 1); 
    int sin_angle_val = sinM7LookupTable[angle_idx];
    int cos_angle_val = cosM7LookupTable[angle_idx];

    int start_screen_line_y = (SCREEN_YSIZE / 2) + 12; 
    byte *current_line_palette_idx_ptr = &gfxLineBuffer[start_screen_line_y]; 
    ushort *frame_buffer_scanline_ptr = &Engine.frameBuffer[start_screen_line_y * GFX_LINESIZE];

    for (int perspective_step = 4; perspective_step < 112; ++perspective_step) { 
        if (!(perspective_step & 1)) {
            if (current_line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) {
                activePalette   = fullPalette[*current_line_palette_idx_ptr];
                activePalette32 = fullPalette32[*current_line_palette_idx_ptr];
            }
            current_line_palette_idx_ptr++; 
        }

        if ((perspective_step << 9) == 0) continue; 

        int XStep = (layer_y_pos_on_screen / (perspective_step << 9)) * -cos_angle_val >> 8; 
        int YStep = (sin_angle_val * (layer_y_pos_on_screen / (perspective_step << 9))) >> 8;
        int XPos_world_start = (layer_x_pos_world >> 4) + (3 * sin_angle_val * (layer_y_pos_on_screen / (perspective_step << 9)) >> 2) - XStep * SCREEN_CENTERX;
        int YPos_world_start = (layer_z_pos_world >> 4) + (3 * cos_angle_val * (layer_y_pos_on_screen / (perspective_step << 9)) >> 2) - YStep * SCREEN_CENTERX;
        
        ushort *fb_pixel_ptr_for_line = frame_buffer_scanline_ptr; 

        for (int screen_x = 0; screen_x < GFX_LINESIZE; ++screen_x) { 
            int current_XPos_world = XPos_world_start + screen_x * XStep; 
            int current_YPos_world = YPos_world_start + screen_x * YStep; 

            int tile_map_x = current_XPos_world >> 16; 
            int tile_map_y = current_YPos_world >> 16;
            
            if (tile_map_x >= 0 && tile_map_x < layer->xsize && tile_map_y >= 0 && tile_map_y < layer->ysize) {
                int chunk_idx_in_tile3Dbuf = (tile_map_y * layer->xsize) + tile_map_x; 
                if (chunk_idx_in_tile3Dbuf < MAX_TILE_CHUNKS_3D) { 
                    int chunk_gfx_id = tile3DFloorBuffer[chunk_idx_in_tile3Dbuf]; 
                    if (chunk_gfx_id < MAX_TILES_IN_SHEET && tiles128x128.gfxDataPos[chunk_gfx_id] < GFXDATA_SIZE) { 
                        int px_in_tile = (current_XPos_world >> 12) & 0xF; 
                        int py_in_tile = (current_YPos_world >> 12) & 0xF;
                        byte *tile_pixel_data_start = &tilesetGFXData[tiles128x128.gfxDataPos[chunk_gfx_id]];
                        byte pixel_color_idx = 0;
                        switch (tiles128x128.direction[chunk_gfx_id]) {
                            case FLIP_NONE: pixel_color_idx = tile_pixel_data_start[py_in_tile * TILE_SIZE + px_in_tile]; break;
                            case FLIP_X:    pixel_color_idx = tile_pixel_data_start[py_in_tile * TILE_SIZE + ((TILE_SIZE - 1) - px_in_tile)]; break;
                            case FLIP_Y:    pixel_color_idx = tile_pixel_data_start[((TILE_SIZE - 1) - py_in_tile) * TILE_SIZE + px_in_tile]; break;
                            case FLIP_XY:   pixel_color_idx = tile_pixel_data_start[((TILE_SIZE - 1) - py_in_tile) * TILE_SIZE + ((TILE_SIZE - 1) - px_in_tile)]; break;
                        }
                        if (pixel_color_idx > 0 && screen_x < GFX_LINESIZE) {
                            fb_pixel_ptr_for_line[screen_x] = activePalette[pixel_color_idx];
                        }
                    }
                }
            }
        }
        frame_buffer_scanline_ptr += GFX_LINESIZE; 
    }
#endif 
}

void Draw3DSkyLayer(int layerID_param) 
{
    if (layerID_param < 0 || layerID_param >= STAGELAYER_COUNT || activeTileLayers[layerID_param] >= LAYER_COUNT) return;
    TileLayer *layer = &stageLayouts[activeTileLayers[layerID_param]];
    if (!layer->xsize || !layer->ysize || layer->type == LAYER_DISABLED || !layer->tiles || !tile3DFloorBuffer) return;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (drawStageGFXHQ && !Engine.frameBuffer2x) return; 

    int layer_y_pos_on_screen = layer->ypos;
    int layer_x_pos_world = layer->xpos;
    int layer_z_pos_world = layer->zpos;
    int angle_idx = layer->angle & (M7_TABLE_SIZE - 1); 
    int sin_angle_val = sinM7LookupTable[angle_idx];
    int cos_angle_val = cosM7LookupTable[angle_idx];

    ushort *target_buffer_base_ptr;
    ushort *read_fb_normal_ptr_base = &Engine.frameBuffer[((SCREEN_YSIZE / 2) + 12) * GFX_LINESIZE]; 
    int current_target_pitch_ushorts;

    if (drawStageGFXHQ) {
        target_buffer_base_ptr = Engine.frameBuffer2x; 
        current_target_pitch_ushorts = GFX_LINESIZE_DOUBLE;
    } else {
        target_buffer_base_ptr = &Engine.frameBuffer[((SCREEN_YSIZE / 2) + 12) * GFX_LINESIZE];
        current_target_pitch_ushorts = GFX_LINESIZE;
    }
    
    byte *current_line_palette_idx_ptr = &gfxLineBuffer[TILE_SIZE / 2]; 

    for (int screen_line_y = TILE_SIZE / 2; screen_line_y < SCREEN_YSIZE - TILE_SIZE; ++screen_line_y) {
        if (!(screen_line_y & 1)) {
            if (current_line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) {
                activePalette   = fullPalette[*current_line_palette_idx_ptr];
                activePalette32 = fullPalette32[*current_line_palette_idx_ptr];
            }
            current_line_palette_idx_ptr++;
        }

        if ((screen_line_y << 8) == 0) continue; 
        int XStep = (layer_y_pos_on_screen / (screen_line_y << 8)) * -cos_angle_val >> 9; 
        int YStep = (sin_angle_val * (layer_y_pos_on_screen / (screen_line_y << 8))) >> 9;
        int view_center_x = (drawStageGFXHQ ? GFX_LINESIZE_DOUBLE : GFX_LINESIZE) / 2;
        int XPos_world_start = (layer_x_pos_world) + (3 * sin_angle_val * (layer_y_pos_on_screen / (screen_line_y << 8)) >> 2) - XStep * view_center_x; 
        int YPos_world_start = (layer_z_pos_world) + (3 * cos_angle_val * (layer_y_pos_on_screen / (screen_line_y << 8)) >> 2) - YStep * view_center_x;

        ushort *target_pixel_ptr_for_line = target_buffer_base_ptr; 
        ushort *read_normal_pixel_ptr_for_line = read_fb_normal_ptr_base;
        int pixels_to_draw_horizontally = drawStageGFXHQ ? (GFX_LINESIZE * 2) : GFX_LINESIZE;

        for (int screen_x_on_line = 0; screen_x_on_line < pixels_to_draw_horizontally; ++screen_x_on_line) {
            int current_XPos_world = XPos_world_start + screen_x_on_line * XStep;
            int current_YPos_world = YPos_world_start + screen_x_on_line * YStep;
            int tile_map_x = current_XPos_world >> 16; 
            int tile_map_y = current_YPos_world >> 16;

            if (tile_map_x >= 0 && tile_map_x < layer->xsize && tile_map_y >= 0 && tile_map_y < layer->ysize) {
                int chunk_idx_in_tile3Dbuf = (tile_map_y * layer->xsize) + tile_map_x; 
                 if (chunk_idx_in_tile3Dbuf < MAX_TILE_CHUNKS_3D) { 
                    int chunk_gfx_id = tile3DFloorBuffer[chunk_idx_in_tile3Dbuf];
                    if (chunk_gfx_id < MAX_TILES_IN_SHEET && tiles128x128.gfxDataPos[chunk_gfx_id] < GFXDATA_SIZE) {
                        int px_in_tile = (current_XPos_world >> 12) & 0xF; 
                        int py_in_tile = (current_YPos_world >> 12) & 0xF;
                        byte *tile_pixel_data_start = &tilesetGFXData[tiles128x128.gfxDataPos[chunk_gfx_id]];
                        byte pixel_color_idx = 0;
                        switch (tiles128x128.direction[chunk_gfx_id]) { /* ... Lógica de FLIP ... */ }
                        if (pixel_color_idx > 0) {
                            target_pixel_ptr_for_line[screen_x_on_line] = activePalette[pixel_color_idx];
                        } else if (drawStageGFXHQ) {
                            target_pixel_ptr_for_line[screen_x_on_line] = read_normal_pixel_ptr_for_line[screen_x_on_line / 2]; 
                        }
                    }
                }
            } else if (drawStageGFXHQ) { 
                target_pixel_ptr_for_line[screen_x_on_line] = read_normal_pixel_ptr_for_line[screen_x_on_line / 2];
            }
        }
        target_buffer_base_ptr += current_target_pitch_ushorts;
        read_fb_normal_ptr_base += GFX_LINESIZE; 
    }

    if (drawStageGFXHQ) { /* ... Rellenar Engine.frameBuffer con magenta ... */ }
#endif 
}

void DrawClassicFade(int XPos, int YPos, int width, int height, 
                     int R_target_8bit, int G_target_8bit, int B_target_8bit, int A_amount) 
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;

    if (XPos < 0) { width += XPos; XPos = 0; } 
    if (YPos < 0) { height += YPos; YPos = 0; }
    if (XPos + width > GFX_LINESIZE) { width = GFX_LINESIZE - XPos; }
    if (YPos + height > SCREEN_YSIZE) { height = SCREEN_YSIZE - YPos; }
    if (width <= 0 || height <= 0 || A_amount <= 0) return;
	
    int fade_step_amount = A_amount;
    // Original RSDKv4: A_amount *= 3; A_amount >>= 3; // Ajusta A_amount a múltiplo de 8 (si positivo)
    // Si este ajuste es crucial para el efecto, descomentar y usar A_amount.
    // if (fade_step_amount == 0 && A_amount > 0) fade_step_amount = 1; // Asegurar paso mínimo

    int framebuffer_pitch = GFX_LINESIZE - width;
    ushort *current_pixel_ptr = &Engine.frameBuffer[XPos + GFX_LINESIZE * YPos];
    ushort target_color_rgb565 = PACK_RGB888(R_target_8bit, G_target_8bit, B_target_8bit);

    int target_R_5bit = (target_color_rgb565 & 0xF800) >> 11;
    int target_G_6bit = (target_color_rgb565 & 0x07E0) >> 5;  
    int target_B_5bit = (target_color_rgb565 & 0x001F);

    for (int h_loop = 0; h_loop < height; ++h_loop) {
        for (int w_loop = 0; w_loop < width; ++w_loop) {
            int diff;
            int current_fade_budget = fade_step_amount; 

            ushort pixel_val_fb = *current_pixel_ptr;
            int current_R_5bit = (pixel_val_fb & 0xF800) >> 11;
            int current_G_6bit = (pixel_val_fb & 0x07E0) >> 5; 
            int current_B_5bit = (pixel_val_fb & 0x001F);
            
            // Componente Rojo
            if (current_fade_budget > 0) {	
                if (target_R_5bit > current_R_5bit) {
                    diff = target_R_5bit - current_R_5bit;
                    if (diff >= current_fade_budget) { current_R_5bit += current_fade_budget; current_fade_budget = 0; }
                    else { current_R_5bit = target_R_5bit; current_fade_budget -= diff; }
                } else if (target_R_5bit < current_R_5bit) {
                    diff = current_R_5bit - target_R_5bit;
                    if (diff >= current_fade_budget) { current_R_5bit -= current_fade_budget; current_fade_budget = 0; }
                    else { current_R_5bit = target_R_5bit; current_fade_budget -= diff; }
                }
            }
            // Componente Verde (6 bits)
            if (current_fade_budget > 0) {	
                if (target_G_6bit > current_G_6bit) {
                    diff = target_G_6bit - current_G_6bit;
                    if (diff >= current_fade_budget) { current_G_6bit += current_fade_budget; current_fade_budget = 0; }
                    else { current_G_6bit = target_G_6bit; current_fade_budget -= diff; }
                } else if (target_G_6bit < current_G_6bit) {
                    diff = current_G_6bit - target_G_6bit;
                    if (diff >= current_fade_budget) { current_G_6bit -= current_fade_budget; current_fade_budget = 0; }
                    else { current_G_6bit = target_G_6bit; current_fade_budget -= diff; }
                }
            }
            // Componente Azul
            if (current_fade_budget > 0) {	
                if (target_B_5bit > current_B_5bit) {
                    diff = target_B_5bit - current_B_5bit;
                    if (diff >= current_fade_budget) { current_B_5bit += current_fade_budget; }
                    else { current_B_5bit = target_B_5bit; }
                } else if (target_B_5bit < current_B_5bit) {
                    diff = current_B_5bit - target_B_5bit;
                    if (diff >= current_fade_budget) { current_B_5bit -= current_fade_budget; }
                    else { current_B_5bit = target_B_5bit; }
                }
            }
            
            *current_pixel_ptr = (ushort)((current_R_5bit << 11) | (current_G_6bit << 5) | current_B_5bit);
            current_pixel_ptr++; 
        }
        current_pixel_ptr += framebuffer_pitch; 
    }
#endif 
}

void DrawRectangle(int XPos, int YPos, int width, int height, 
                   int R_param, int G_param, int B_param, int A_param) 
{
    if (A_param > 0xFF) A_param = 0xFF;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;

    if (XPos < 0) { width += XPos; XPos = 0; } 
    if (YPos < 0) { height += YPos; YPos = 0; }
    if (XPos + width > GFX_LINESIZE) { width = GFX_LINESIZE - XPos; }
    if (YPos + height > SCREEN_YSIZE) { height = SCREEN_YSIZE - YPos; }
    if (width <= 0 || height <= 0 || A_param <= 0) return;

    int framebuffer_pitch = GFX_LINESIZE - width;
    ushort *current_pixel_ptr = &Engine.frameBuffer[XPos + GFX_LINESIZE * YPos];
    ushort rect_color_rgb565 = PACK_RGB888(R_param, G_param, B_param);

    if (A_param == 0xFF) { 
        for (int h_loop = 0; h_loop < height; ++h_loop) {
            for (int w_loop = 0; w_loop < width; ++w_loop) {
                *current_pixel_ptr++ = rect_color_rgb565;
            }
            current_pixel_ptr += framebuffer_pitch;
        }
    } else { 
        ushort *fbufferBlend_table_ptr = &blendLookupTable[0x20 * (0xFF - A_param)];
        ushort *pixelBlend_table_ptr   = &blendLookupTable[0x20 * A_param];

        int rect_R_5bit = (rect_color_rgb565 & 0xF800) >> 11;
        int rect_G_6bit = (rect_color_rgb565 & 0x07E0) >> 5;
        int rect_B_5bit = (rect_color_rgb565 & 0x001F);
        int rect_G_5bit_for_table = rect_G_6bit >> 1;

        for (int h_loop = 0; h_loop < height; ++h_loop) {
            for (int w_loop = 0; w_loop < width; ++w_loop) {
                ushort fb_pixel_val = *current_pixel_ptr;
                int fb_R_5bit = (fb_pixel_val & 0xF800) >> 11;
                int fb_G_6bit = (fb_pixel_val & 0x07E0) >> 5;
                int fb_B_5bit = (fb_pixel_val & 0x001F);
                int fb_G_5bit_for_table = fb_G_6bit >> 1;

                int blended_R_5bit = fbufferBlend_table_ptr[fb_R_5bit] + pixelBlend_table_ptr[rect_R_5bit];
                int blended_G_5bit = fbufferBlend_table_ptr[fb_G_5bit_for_table] + pixelBlend_table_ptr[rect_G_5bit_for_table];
                int blended_B_5bit = fbufferBlend_table_ptr[fb_B_5bit] + pixelBlend_table_ptr[rect_B_5bit];
                
                if (blended_R_5bit > 31) blended_R_5bit = 31;
                if (blended_G_5bit > 31) blended_G_5bit = 31; 
                if (blended_B_5bit > 31) blended_B_5bit = 31;
                
                int final_G_6bit = blended_G_5bit << 1; 
                if (final_G_6bit > 63) final_G_6bit = 63; 

                *current_pixel_ptr = (ushort)((blended_R_5bit << 11) | (final_G_6bit << 5) | blended_B_5bit);
                current_pixel_ptr++;
            }
            current_pixel_ptr += framebuffer_pitch;
        }
    }
#endif 
}

void SetFadeHQ(int R_param, int G_param, int B_param, int A_param) 
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer2x) { 
        return; 
    }

    if (A_param <= 0) return;
    if (A_param > 0xFF) A_param = 0xFF;

    int hq_pitch_ushorts = GFX_LINESIZE_DOUBLE; 
    ushort *fb_hq_ptr = Engine.frameBuffer2x; 
    ushort target_color_rgb565 = PACK_RGB888(R_param, G_param, B_param);
    int loop_height = SCREEN_YSIZE * 2; // Asume que se quiere afectar todo el alto del buffer HQ

    if (A_param == 0xFF) { 
        for (int y = 0; y < loop_height; ++y) {
            ushort* current_line_ptr = fb_hq_ptr + (y * hq_pitch_ushorts);
            for (int x = 0; x < hq_pitch_ushorts; ++x) {
                current_line_ptr[x] = target_color_rgb565;
            }
        }
    } else { 
        ushort *fbufferBlend_table_ptr = &blendLookupTable[0x20 * (0xFF - A_param)];
        ushort *pixelBlend_table_ptr   = &blendLookupTable[0x20 * A_param];

        int target_R_5bit = (target_color_rgb565 & 0xF800) >> 11;
        int target_G_6bit = (target_color_rgb565 & 0x07E0) >> 5;
        int target_B_5bit = (target_color_rgb565 & 0x001F);
        int target_G_5bit_for_table = target_G_6bit >> 1;

        for (int y = 0; y < loop_height; ++y) {
            ushort* current_line_ptr = fb_hq_ptr + (y * hq_pitch_ushorts);
            for (int x = 0; x < hq_pitch_ushorts; ++x) {
                ushort fb_pixel_val = current_line_ptr[x];
                int fb_R_5bit = (fb_pixel_val & 0xF800) >> 11;
                int fb_G_6bit = (fb_pixel_val & 0x07E0) >> 5;
                int fb_B_5bit = (fb_pixel_val & 0x001F);
                int fb_G_5bit_for_table = fb_G_6bit >> 1;

                int blended_R_5bit = fbufferBlend_table_ptr[fb_R_5bit] + pixelBlend_table_ptr[target_R_5bit];
                int blended_G_5bit = fbufferBlend_table_ptr[fb_G_5bit_for_table] + pixelBlend_table_ptr[target_G_5bit_for_table];
                int blended_B_5bit = fbufferBlend_table_ptr[fb_B_5bit] + pixelBlend_table_ptr[target_B_5bit];
                
                if (blended_R_5bit > 31) blended_R_5bit = 31;
                if (blended_G_5bit > 31) blended_G_5bit = 31; 
                if (blended_B_5bit > 31) blended_B_5bit = 31;
                int final_G_6bit = blended_G_5bit << 1; 
                if (final_G_6bit > 63) final_G_6bit = 63; 

                current_line_ptr[x] = (ushort)((blended_R_5bit << 11) | (final_G_6bit << 5) | blended_B_5bit);
            }
        }
    }
#endif 
}

void DrawTintRectangle(int XPos, int YPos, int width, int height)
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;

    if (XPos < 0) { width += XPos; XPos = 0; } 
    if (YPos < 0) { height += YPos; YPos = 0; }
    if (XPos + width > GFX_LINESIZE) { width = GFX_LINESIZE - XPos; }
    if (YPos + height > SCREEN_YSIZE) { height = SCREEN_YSIZE - YPos; }
    if (width <= 0 || height <= 0) return;

    int framebuffer_pitch_to_next_line = GFX_LINESIZE - width; 
    ushort *current_pixel_ptr = &Engine.frameBuffer[XPos + GFX_LINESIZE * YPos];

    for (int h_loop = 0; h_loop < height; ++h_loop) {
        for (int w_loop = 0; w_loop < width; ++w_loop) {
            *current_pixel_ptr = tintLookupTable[*current_pixel_ptr];
            current_pixel_ptr++; 
        }
        current_pixel_ptr += framebuffer_pitch_to_next_line; 
    }
#endif 
}

void DrawScaledTintMask(int direction_flip, int XPos_screen, int YPos_screen,
                        int pivotX_sprite, int pivotY_sprite,
                        int scaleX_factor, int scaleY_factor,
                        int frame_width_orig, int frame_height_orig, 
                        int sprX_in_sheet, int sprY_in_sheet,
                        int sheetID)
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;
    GFXSurface *surface_ptr = &gfxSurface[sheetID];

    // --- CORRECCIÓN IMPORTANTE AQUÍ ---
    if (surface_ptr->dataPosition >= GFXDATA_SIZE) { 
        return; 
    }
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];
    // --- FIN DE LA CORRECCIÓN ---

    long long truescaleX_ll = (long long)4 * scaleX_factor;
    long long truescaleY_ll = (long long)4 * scaleY_factor;
    int screen_rect_X = XPos_screen - (int)((truescaleX_ll * pivotX_sprite) >> 11);
    int screen_rect_Y = YPos_screen - (int)((truescaleY_ll * pivotY_sprite) >> 11);
    int screen_rect_width  = (int)((truescaleX_ll * frame_width_orig) >> 11);
    int screen_rect_height = (int)((truescaleY_ll * frame_height_orig) >> 11);

    if (screen_rect_width <= 0 || screen_rect_height <= 0) return;

    int finalscaleX_fixed = (truescaleX_ll != 0) ? (int)((2048.0f / (float)truescaleX_ll) * 2048.0f) : (1<<11);
    int finalscaleY_fixed = (truescaleY_ll != 0) ? (int)((2048.0f / (float)truescaleY_ll) * 2048.0f) : (1<<11);
    if (finalscaleX_fixed == 0 && screen_rect_width > 0) finalscaleX_fixed = (1<<11);
    if (finalscaleY_fixed == 0 && screen_rect_height > 0) finalscaleY_fixed = (1<<11);

    int current_sprX_tex = sprX_in_sheet;
    int current_sprY_tex = sprY_in_sheet;
    int subpixel_X_accumulator = 0;
    int subpixel_Y_accumulator = 0;
    int frame_w_for_flip = frame_width_orig -1;

    int draw_w = screen_rect_width;
    int draw_h = screen_rect_height;
    int current_screen_draw_X = screen_rect_X;
    int current_screen_draw_Y = screen_rect_Y;

    // --- Clipping ---
    if (current_screen_draw_X < 0) {
        int offset = -current_screen_draw_X;
        long long tex_adv = (long long)offset * finalscaleX_fixed;
        if (direction_flip == FLIP_X) frame_w_for_flip -= (int)(tex_adv >> 11);
        else current_sprX_tex += (int)(tex_adv >> 11);
        subpixel_X_accumulator = (int)(tex_adv & 0x7FF);
        draw_w -= offset; current_screen_draw_X = 0;
    }
    if (current_screen_draw_X + draw_w > GFX_LINESIZE) draw_w = GFX_LINESIZE - current_screen_draw_X;
    if (current_screen_draw_Y < 0) {
        int offset = -current_screen_draw_Y;
        long long tex_adv = (long long)offset * finalscaleY_fixed;
        current_sprY_tex += (int)(tex_adv >> 11);
        subpixel_Y_accumulator = (int)(tex_adv & 0x7FF);
        draw_h -= offset; current_screen_draw_Y = 0;
    }
    if (current_screen_draw_Y + draw_h > SCREEN_YSIZE) draw_h = SCREEN_YSIZE - current_screen_draw_Y;
    if (draw_w <= 0 || draw_h <= 0) return;
    // --- Fin Clipping ---

    ushort *current_screen_line_ptr = &Engine.frameBuffer[current_screen_draw_X + GFX_LINESIZE * current_screen_draw_Y];
    long long tex_coord_Y_fixed_current_line = ((long long)current_sprY_tex << 11) + subpixel_Y_accumulator;
    int spritesheet_actual_width = surface_ptr->width;

    for (int y_loop = 0; y_loop < draw_h; ++y_loop) {
        int tex_y_int = (int)(tex_coord_Y_fixed_current_line >> 11);
        if (tex_y_int >= sprY_in_sheet && tex_y_int < sprY_in_sheet + frame_height_orig) {
            long long tex_coord_X_fixed_pixel;
            int tex_x_step_direction_sign = (direction_flip == FLIP_X) ? -1 : 1;
            if (direction_flip == FLIP_X) {
                tex_coord_X_fixed_pixel = ((long long)(current_sprX_tex + frame_w_for_flip) << 11) + subpixel_X_accumulator;
            } else {
                tex_coord_X_fixed_pixel = ((long long)current_sprX_tex << 11) + subpixel_X_accumulator;
            }
            ushort *screen_px_write_ptr = current_screen_line_ptr;
            for (int x_loop = 0; x_loop < draw_w; ++x_loop) {
                int tex_x_int = (int)(tex_coord_X_fixed_pixel >> 11);
                if (tex_x_int >= sprX_in_sheet && tex_x_int < sprX_in_sheet + frame_width_orig) {
                    int sprite_pixel_idx = (tex_y_int * spritesheet_actual_width) + tex_x_int;
                    if (sprite_pixel_idx >= 0 && sprite_pixel_idx < (spritesheet_actual_width * surface_ptr->height)) {
                        byte sprite_pixel_val = sprite_pixel_data_origin[sprite_pixel_idx];
                        if (sprite_pixel_val > 0) {
                            *screen_px_write_ptr = tintLookupTable[*screen_px_write_ptr];
                        }
                    }
                }
                screen_px_write_ptr++;
                tex_coord_X_fixed_pixel += (long long)finalscaleX_fixed * tex_x_step_direction_sign;
            }
        }
        current_screen_line_ptr += GFX_LINESIZE;
        tex_coord_Y_fixed_current_line += finalscaleY_fixed;
    }
#endif 
}

void DrawSprite(int XPos_screen, int YPos_screen, 
                int frame_width, int frame_height, 
                int sprX_in_sheet, int sprY_in_sheet, 
                int sheetID)
{
    #ifdef PS3_PPU_PRX_LOADER
    printf("PS3_DEBUG: DrawSprite(X: %d, Y: %d, W: %d, H: %d, SX: %d, SY: %d, Sheet: %d) called.\n",
           XPos_screen, YPos_screen, frame_width, frame_height, sprX_in_sheet, sprY_in_sheet, sheetID);
    #endif
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;

    int current_XPos_screen = XPos_screen;
    int current_YPos_screen = YPos_screen;
    int current_frame_width = frame_width;
    int current_frame_height = frame_height;
    int current_sprX_in_sheet = sprX_in_sheet;
    int current_sprY_in_sheet = sprY_in_sheet;

    if (current_XPos_screen < 0) {
        current_sprX_in_sheet -= current_XPos_screen; 
        current_frame_width += current_XPos_screen;   
        current_XPos_screen = 0;                      
    }
    if (current_YPos_screen < 0) {
        current_sprY_in_sheet -= current_YPos_screen;
        current_frame_height += current_YPos_screen;
        current_YPos_screen = 0;
    }
    if (current_XPos_screen + current_frame_width > GFX_LINESIZE) {
        current_frame_width = GFX_LINESIZE - current_XPos_screen;
    }
    if (current_YPos_screen + current_frame_height > SCREEN_YSIZE) {
        current_frame_height = SCREEN_YSIZE - current_YPos_screen;
    }

    if (current_frame_width <= 0 || current_frame_height <= 0) return;

    GFXSurface *surface_ptr = &gfxSurface[sheetID];
    if (surface_ptr->dataPosition >= GFXDATA_SIZE) return;
    // Asumiendo que GFXSurface no tiene ->pixels y se usa graphicData globalmente:
    // byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];
    // Si GFXSurface SÍ tiene un puntero a sus propios píxeles (ej. cargados desde graphicData)
    // byte *sprite_pixel_data_origin = surface_ptr->pixels; 
    // Por ahora, seguimos la lógica original que implica un offset en graphicData:
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];


    int framebuffer_pitch = GFX_LINESIZE - current_frame_width;
    int spritesheet_pitch = surface_ptr->width - current_frame_width;
    byte *line_palette_idx_ptr = &gfxLineBuffer[current_YPos_screen];
    byte *sprite_pixel_ptr = &sprite_pixel_data_origin[current_sprX_in_sheet + surface_ptr->width * current_sprY_in_sheet];
    ushort *screen_pixel_ptr = &Engine.frameBuffer[current_XPos_screen + GFX_LINESIZE * current_YPos_screen];

    for (int y_loop = 0; y_loop < current_frame_height; ++y_loop) {
        if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) { 
            activePalette   = fullPalette[*line_palette_idx_ptr];
        }
        line_palette_idx_ptr++;

        for (int x_loop = 0; x_loop < current_frame_width; ++x_loop) {
            byte sprite_pixel_color_index = *sprite_pixel_ptr;
            if (sprite_pixel_color_index > 0) { 
                *screen_pixel_ptr = activePalette[sprite_pixel_color_index];
            }
            sprite_pixel_ptr++;
            screen_pixel_ptr++;
        }
        screen_pixel_ptr += framebuffer_pitch;   
        sprite_pixel_ptr += spritesheet_pitch; 
    }
#endif 
}

#if RETRO_REV00 
void DrawSpriteClipped(int XPos_screen, int YPos_screen, 
                       int frame_width, int frame_height, 
                       int sprX_in_sheet, int sprY_in_sheet, 
                       int sheetID, 
                       int clip_Y_bottom)
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;

    int current_XPos_screen = XPos_screen;
    int current_YPos_screen = YPos_screen;
    int current_frame_width = frame_width;
    int current_frame_height = frame_height;
    int current_sprX_in_sheet = sprX_in_sheet;
    int current_sprY_in_sheet = sprY_in_sheet;

    if (current_XPos_screen < 0) {
        current_sprX_in_sheet -= current_XPos_screen;
        current_frame_width += current_XPos_screen;
        current_XPos_screen = 0;
    }
    if (current_XPos_screen + current_frame_width > GFX_LINESIZE) {
        current_frame_width = GFX_LINESIZE - current_XPos_screen;
    }
    if (current_YPos_screen < 0) {
        current_sprY_in_sheet -= current_YPos_screen;
        current_frame_height += current_YPos_screen;
        current_YPos_screen = 0;
    }
    if (current_YPos_screen + current_frame_height > clip_Y_bottom) {
        current_frame_height = clip_Y_bottom - current_YPos_screen;
    }
    if (current_YPos_screen + current_frame_height > SCREEN_YSIZE) { // Asegurar que no exceda la pantalla
        current_frame_height = SCREEN_YSIZE - current_YPos_screen;
    }

    if (current_frame_width <= 0 || current_frame_height <= 0) return;

    GFXSurface *surface_ptr = &gfxSurface[sheetID];
    if (surface_ptr->dataPosition >= GFXDATA_SIZE) return; 
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];

    int framebuffer_pitch = GFX_LINESIZE - current_frame_width;
    int spritesheet_pitch = surface_ptr->width - current_frame_width;
    byte *line_palette_idx_ptr = &gfxLineBuffer[current_YPos_screen];
    byte *sprite_pixel_ptr = &sprite_pixel_data_origin[current_sprX_in_sheet + surface_ptr->width * current_sprY_in_sheet];
    ushort *screen_pixel_ptr = &Engine.frameBuffer[current_XPos_screen + GFX_LINESIZE * current_YPos_screen];

    for (int y_loop = 0; y_loop < current_frame_height; ++y_loop) {
        if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) { 
            activePalette   = fullPalette[*line_palette_idx_ptr];
        }
        line_palette_idx_ptr++;

        for (int x_loop = 0; x_loop < current_frame_width; ++x_loop) {
            byte sprite_pixel_color_index = *sprite_pixel_ptr;
            if (sprite_pixel_color_index > 0) { 
                *screen_pixel_ptr = activePalette[sprite_pixel_color_index];
            }
            sprite_pixel_ptr++;
            screen_pixel_ptr++;
        }
        screen_pixel_ptr += framebuffer_pitch;
        sprite_pixel_ptr += spritesheet_pitch;
    }
#endif 
}
#endif

void DrawSpriteFlipped(int XPos_screen, int YPos_screen, 
                       int frame_width, int frame_height, 
                       int sprX_in_sheet, int sprY_in_sheet, 
                       int direction, int sheetID)
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;

    int original_frame_w = frame_width;
    int original_frame_h = frame_height;

    int screen_x = XPos_screen;
    int screen_y = YPos_screen;
    int w_to_draw = frame_width;
    int h_to_draw = frame_height;
    int sheet_spr_x = sprX_in_sheet;
    int sheet_spr_y = sprY_in_sheet;

    // --- Clipping (basado en tu lógica original) ---
    if (screen_x + w_to_draw > GFX_LINESIZE) w_to_draw = GFX_LINESIZE - screen_x;
    if (screen_x < 0) { sheet_spr_x -= screen_x; w_to_draw += screen_x; screen_x = 0;}
    if (screen_y + h_to_draw > SCREEN_YSIZE) h_to_draw = SCREEN_YSIZE - screen_y;
    if (screen_y < 0) { sheet_spr_y -= screen_y; h_to_draw += screen_y; screen_y = 0;}
    if (w_to_draw <= 0 || h_to_draw <= 0) return;
    // --- Fin Clipping ---

    GFXSurface *surface_ptr = &gfxSurface[sheetID];
    if (surface_ptr->dataPosition >= GFXDATA_SIZE || 
        (surface_ptr->dataPosition + surface_ptr->width * surface_ptr->height > GFXDATA_SIZE) ) return;
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];

    byte *line_palette_idx_ptr = &gfxLineBuffer[screen_y];
    ushort *screen_px_current_line_start = &Engine.frameBuffer[screen_x + GFX_LINESIZE * screen_y];

    switch (direction) {
        case FLIP_NONE: {
            byte *sprite_line_start = &sprite_pixel_data_origin[sheet_spr_x + surface_ptr->width * sheet_spr_y];
            for (int y = 0; y < h_to_draw; ++y) {
                if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
                line_palette_idx_ptr++;
                byte *sprite_px = sprite_line_start;
                ushort *screen_px = screen_px_current_line_start;
                for (int x = 0; x < w_to_draw; ++x) {
                    if (*sprite_px > 0) *screen_px = activePalette[*sprite_px];
                    sprite_px++; screen_px++;
                }
                screen_px_current_line_start += GFX_LINESIZE;
                sprite_line_start += surface_ptr->width;
            }
            break;
        }
        case FLIP_X: {
            byte *sprite_line_start = &sprite_pixel_data_origin[sheet_spr_x + (original_frame_w - 1) + surface_ptr->width * sheet_spr_y];
            for (int y = 0; y < h_to_draw; ++y) {
                if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
                line_palette_idx_ptr++;
                byte *sprite_px = sprite_line_start;
                ushort *screen_px = screen_px_current_line_start;
                for (int x = 0; x < w_to_draw; ++x) {
                    if (*sprite_px > 0) *screen_px = activePalette[*sprite_px];
                    sprite_px--; screen_px++;
                }
                screen_px_current_line_start += GFX_LINESIZE;
                sprite_line_start += surface_ptr->width;
            }
            break;
        }
        case FLIP_Y: {
            byte *sprite_line_start = &sprite_pixel_data_origin[sheet_spr_x + surface_ptr->width * (sheet_spr_y + original_frame_h - 1)];
            for (int y = 0; y < h_to_draw; ++y) {
                if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
                line_palette_idx_ptr++;
                byte *sprite_px = sprite_line_start;
                ushort *screen_px = screen_px_current_line_start;
                for (int x = 0; x < w_to_draw; ++x) {
                    if (*sprite_px > 0) *screen_px = activePalette[*sprite_px];
                    sprite_px++; screen_px++;
                }
                screen_px_current_line_start += GFX_LINESIZE;
                sprite_line_start -= surface_ptr->width;
            }
            break;
        }
        case FLIP_XY: {
            byte *sprite_line_start = &sprite_pixel_data_origin[sheet_spr_x + (original_frame_w - 1) + surface_ptr->width * (sheet_spr_y + original_frame_h - 1)];
            for (int y = 0; y < h_to_draw; ++y) {
                if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
                line_palette_idx_ptr++;
                byte *sprite_px = sprite_line_start;
                ushort *screen_px = screen_px_current_line_start;
                for (int x = 0; x < w_to_draw; ++x) {
                    if (*sprite_px > 0) *screen_px = activePalette[*sprite_px];
                    sprite_px--; screen_px++;
                }
                screen_px_current_line_start += GFX_LINESIZE;
                sprite_line_start -= surface_ptr->width;
            }
            break;
        }
        default: break;
    }
#endif 
}

void DrawSpriteScaled(int direction_flip, int XPos_screen, int YPos_screen, 
                      int pivotX_sprite, int pivotY_sprite, 
                      int scaleX_factor, int scaleY_factor, 
                      int frame_width_orig, int frame_height_orig, 
                      int sprX_in_sheet, int sprY_in_sheet,
                      int sheetID)
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;
    GFXSurface *surface_ptr = &gfxSurface[sheetID];

    if (surface_ptr->dataPosition >= GFXDATA_SIZE) return; 
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];

    long long truescaleX_param = (long long)4 * scaleX_factor;
    long long truescaleY_param = (long long)4 * scaleY_factor;

    int screen_spr_width  = (int)(((long long)scaleX_factor * frame_width_orig) >> 11);
    int screen_spr_height = (int)(((long long)scaleY_factor * frame_height_orig) >> 11);
    int screen_draw_X = XPos_screen - (int)(((long long)scaleX_factor * pivotX_sprite) >> 11);
    int screen_draw_Y = YPos_screen - (int)(((long long)scaleY_factor * pivotY_sprite) >> 11);

    if (screen_spr_width <= 0 || screen_spr_height <= 0) return;

    int tex_step_X_fixed = (truescaleX_param != 0) ? (int)((2048.0f / (float)truescaleX_param) * 2048.0f) : 0;
    int tex_step_Y_fixed = (truescaleY_param != 0) ? (int)((2048.0f / (float)truescaleY_param) * 2048.0f) : 0;
    if (tex_step_X_fixed == 0 && screen_spr_width > 0) tex_step_X_fixed = (1<<11); // Evitar paso cero, 1.0 en punto fijo
    if (tex_step_Y_fixed == 0 && screen_spr_height > 0) tex_step_Y_fixed = (1<<11);

    int current_sprX_tex = sprX_in_sheet;
    int current_sprY_tex = sprY_in_sheet;
    int subpixel_X_accumulator = 0; 
    int subpixel_Y_accumulator = 0; 
    int frame_w_for_flip_logic = frame_width_orig -1;

    int draw_w = screen_spr_width;
    int draw_h = screen_spr_height;
    int current_screen_draw_X = screen_draw_X;
    int current_screen_draw_Y = screen_draw_Y;

    // --- Clipping ---
    if (current_screen_draw_X < 0) {
        int offset = -current_screen_draw_X;
        long long tex_adv = (long long)offset * tex_step_X_fixed;
        if (direction_flip == FLIP_X) frame_w_for_flip_logic -= (int)(tex_adv >> 11);
        else current_sprX_tex += (int)(tex_adv >> 11);
        subpixel_X_accumulator = (int)(tex_adv & 0x7FF);
        draw_w -= offset; current_screen_draw_X = 0;
    }
    if (current_screen_draw_X + draw_w > GFX_LINESIZE) draw_w = GFX_LINESIZE - current_screen_draw_X;
    if (current_screen_draw_Y < 0) {
        int offset = -current_screen_draw_Y;
        long long tex_adv = (long long)offset * tex_step_Y_fixed;
        current_sprY_tex += (int)(tex_adv >> 11);
        subpixel_Y_accumulator = (int)(tex_adv & 0x7FF);
        draw_h -= offset; current_screen_draw_Y = 0;
    }
    if (current_screen_draw_Y + draw_h > SCREEN_YSIZE) draw_h = SCREEN_YSIZE - current_screen_draw_Y;
    if (draw_w <= 0 || draw_h <= 0) return;
    // --- Fin Clipping ---

    ushort *screen_pixel_line_start_ptr = &Engine.frameBuffer[current_screen_draw_X + GFX_LINESIZE * current_screen_draw_Y];
    byte *line_palette_idx_ptr = &gfxLineBuffer[current_screen_draw_Y];
    long long tex_coord_Y_fixed_current_line = ((long long)current_sprY_tex << 11) + subpixel_Y_accumulator;
    int spritesheet_width_px = surface_ptr->width;

    for (int y_loop = 0; y_loop < draw_h; ++y_loop) {
        if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
        line_palette_idx_ptr++;
        int tex_y_int = (int)(tex_coord_Y_fixed_current_line >> 11);

        if (tex_y_int >= sprY_in_sheet && tex_y_int < sprY_in_sheet + frame_height_orig) {
            long long tex_coord_X_fixed_pixel;
            int tex_x_step_dir_sign = (direction_flip == FLIP_X) ? -1 : 1;
            if (direction_flip == FLIP_X) {
                tex_coord_X_fixed_pixel = ((long long)(current_sprX_tex + frame_w_for_flip_logic) << 11) + subpixel_X_accumulator;
            } else {
                tex_coord_X_fixed_pixel = ((long long)current_sprX_tex << 11) + subpixel_X_accumulator;
            }
            ushort *screen_px_write_ptr = screen_pixel_line_start_ptr;
            for (int x_loop = 0; x_loop < draw_w; ++x_loop) {
                int tex_x_int = (int)(tex_coord_X_fixed_pixel >> 11);
                if (tex_x_int >= sprX_in_sheet && tex_x_int < sprX_in_sheet + frame_width_orig) {
                    int sprite_pixel_idx = (tex_y_int * spritesheet_width_px) + tex_x_int;
                    if (sprite_pixel_idx >= 0 && sprite_pixel_idx < (spritesheet_width_px * surface_ptr->height)) {
                         byte sprite_color_idx = sprite_pixel_data_origin[sprite_pixel_idx];
                         if (sprite_color_idx > 0) *screen_px_write_ptr = activePalette[sprite_color_idx];
                    }
                }
                screen_px_write_ptr++;
                tex_coord_X_fixed_pixel += (long long)tex_step_X_fixed * tex_x_step_dir_sign;
            }
        }
        screen_pixel_line_start_ptr += GFX_LINESIZE;
        tex_coord_Y_fixed_current_line += tex_step_Y_fixed;
    }
#endif 
}

#if RETRO_REV00 || RETRO_REV01 
void DrawScaledChar(int direction, int XPos, int YPos, 
                    int pivotX, int pivotY, 
                    int scaleX, int scaleY, 
                    int width, int height, 
                    int sprX, int sprY,
                    int sheetID)
{
#if RETRO_SOFTWARE_RENDER
    // Esta función está marcada como no disponible en modo de renderizado por software
    // en el código original que proporcionaste.
    // Si se necesitara, requeriría una implementación similar a DrawSpriteScaled.
    // Para PS3, si se llama, no tendrá efecto.
#endif
}
#endif 

void DrawSpriteRotated(int direction_flip, int XPos_screen, int YPos_screen, 
                       int pivotX_sprite, int pivotY_sprite, 
                       int sprX_in_sheet, int sprY_in_sheet, 
                       int frame_width, int frame_height, 
                       int rotation_angle, int sheetID)
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;
    GFXSurface *surface_ptr = &gfxSurface[sheetID];

    // --- CORRECCIÓN APLICADA ---
    if (surface_ptr->dataPosition >= GFXDATA_SIZE) return; 
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];
    // --- FIN CORRECCIÓN ---

    long long sprX_tex_pivot_fixed = (long long)(pivotX_sprite + sprX_in_sheet) << 9;
    long long sprY_tex_pivot_fixed = (long long)(pivotY_sprite + sprY_in_sheet) << 9;
    int angle_idx = rotation_angle & 0x1FF;
    if (angle_idx != 0) angle_idx = 0x200 - angle_idx; 
    long long sin_val_fixed = sin512LookupTable[angle_idx]; 
    long long cos_val_fixed = cos512LookupTable[angle_idx]; 

    // --- Cálculo del Bounding Box ---
    // (La lógica original de RSDK para calcular las esquinas transformadas con FLIP es compleja.
    //  Esta es una versión simplificada del cálculo de esquinas para el bounding box).
    long long corners_lx[4]={-pivotX_sprite, frame_width-pivotX_sprite, -pivotX_sprite, frame_width-pivotX_sprite};
    long long corners_ly[4]={-pivotY_sprite, -pivotY_sprite, frame_height-pivotY_sprite, frame_height-pivotY_sprite};
    int screen_corners_x[4], screen_corners_y[4];
    for(int i=0;i<4;++i){
        long long lx=corners_lx[i], ly=corners_ly[i];
        screen_corners_x[i] = XPos_screen + (int)((sin_val_fixed * ly + cos_val_fixed * lx) >> 9);
        screen_corners_y[i] = YPos_screen + (int)((cos_val_fixed * ly - sin_val_fixed * lx) >> 9);
    }
    int bb_left=GFX_LINESIZE,bb_right=0,bb_top=SCREEN_YSIZE,bb_bottom=0;
    for(int i=0;i<4;++i){if(screen_corners_x[i]<bb_left)bb_left=screen_corners_x[i]; if(screen_corners_x[i]>bb_right)bb_right=screen_corners_x[i]; if(screen_corners_y[i]<bb_top)bb_top=screen_corners_y[i]; if(screen_corners_y[i]>bb_bottom)bb_bottom=screen_corners_y[i];}
    if(bb_left<0)bb_left=0; if(bb_right>GFX_LINESIZE)bb_right=GFX_LINESIZE;
    if(bb_top<0)bb_top=0; if(bb_bottom>SCREEN_YSIZE)bb_bottom=SCREEN_YSIZE;
    int draw_w = bb_right-bb_left, draw_h = bb_bottom-bb_top;
    if(draw_w<=0||draw_h<=0)return;
    // --- Fin Bounding Box ---

    ushort *screen_line_ptr = &Engine.frameBuffer[bb_left + GFX_LINESIZE * bb_top];
    byte *palette_line_ptr = &gfxLineBuffer[bb_top];
    int screen_rel_X0 = bb_left - XPos_screen, screen_rel_Y0 = bb_top - YPos_screen;
    int sheet_width = surface_ptr->width;

    for (int y = 0; y < draw_h; ++y) {
        if (palette_line_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*palette_line_ptr];
        palette_line_ptr++;
        ushort *screen_px = screen_line_ptr;
        long long cur_screen_Y_rel_fx = (long long)(screen_rel_Y0 + y) << 9;
        for (int x = 0; x < draw_w; ++x) {
            long long cur_screen_X_rel_fx = (long long)(screen_rel_X0 + x) << 9;
            long long rot_loc_X = (cur_screen_X_rel_fx * cos_val_fixed + cur_screen_Y_rel_fx * sin_val_fixed) >> 9;
            long long rot_loc_Y = (cur_screen_Y_rel_fx * cos_val_fixed - cur_screen_X_rel_fx * sin_val_fixed) >> 9;
            long long tex_X_fx = sprX_tex_pivot_fixed + ( (direction_flip == FLIP_X) ? -rot_loc_X : rot_loc_X );
            long long tex_Y_fx = sprY_tex_pivot_fixed + rot_loc_Y;
            int tex_x_i = (int)(tex_X_fx >> 9), tex_y_i = (int)(tex_Y_fx >> 9);
            if (tex_x_i >= sprX_in_sheet && tex_x_i < sprX_in_sheet + frame_width && tex_y_i >= sprY_in_sheet && tex_y_i < sprY_in_sheet + frame_height) {
                int spr_idx = (tex_y_i * sheet_width) + tex_x_i;
                if (spr_idx >= 0 && spr_idx < (sheet_width * surface_ptr->height)) {
                    byte cidx = sprite_pixel_data_origin[spr_idx];
                    if (cidx > 0) *screen_px = activePalette[cidx];
                }
            }
            screen_px++;
        }
        screen_line_ptr += GFX_LINESIZE;
    }
#endif 
}

void DrawSpriteRotozoom(int direction, int XPos, int YPos, 
                        int pivotX, int pivotY, 
                        int sprX, int sprY, 
                        int width, int height, 
                        int rotation, int scale,
                        int sheetID)
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;
    GFXSurface *surface = &gfxSurface[sheetID]; // Tu nombre de variable

    if (surface->dataPosition >= GFXDATA_SIZE) return; 
    byte *gfxData = &graphicData[surface->dataPosition]; // Corregido

    if (scale == 0) return;

    int sprXPos    = (pivotX + sprX) << 9;
    int sprYPos    = (pivotY + sprY) << 9;
    // Límites de textura en punto fijo (usando width/height del frame original)
    long long fullwidth_fixed  = (long long)(sprX + width) << 9;
    long long fullheight_fixed = (long long)(sprY + height) << 9;
    long long shiftPivot_fixed   = (long long)sprX << 9;
    long long shiftheight_fixed  = (long long)sprY << 9;
    shiftPivot_fixed -=1; 
    shiftheight_fixed -=1;

    int angle      = rotation & 0x1FF;
    if (angle != 0) angle = 0x200 - angle;

    // Seno/Coseno para el bounding box (pre-escalados por 'scale')
    int sine_bb   = scale * sin512LookupTable[angle] >> 9;
    int cosine_bb = scale * cos512LookupTable[angle] >> 9;
    int xPositions[4];
    int yPositions[4];

    // --- TU CÁLCULO ORIGINAL DEL BOUNDING BOX --- 
    if (direction == FLIP_X) {
        xPositions[0] = XPos + ((sine_bb * (-pivotY - 2) + cosine_bb * (pivotX + 2)) >> 9);
        yPositions[0] = YPos + ((cosine_bb * (-pivotY - 2) - sine_bb * (pivotX + 2)) >> 9);
        xPositions[1] = XPos + ((sine_bb * (-pivotY - 2) + cosine_bb * (pivotX - width - 2)) >> 9);
        yPositions[1] = YPos + ((cosine_bb * (-pivotY - 2) - sine_bb * (pivotX - width - 2)) >> 9);
        xPositions[2] = XPos + ((sine_bb * (height - pivotY + 2) + cosine_bb * (pivotX + 2)) >> 9);
        yPositions[2] = YPos + ((cosine_bb * (height - pivotY + 2) - sine_bb * (pivotX + 2)) >> 9);
        int a = pivotX - width - 2; int b = height - pivotY + 2;
        xPositions[3] = XPos + ((sine_bb * b + cosine_bb * a) >> 9);
        yPositions[3] = YPos + ((cosine_bb * b - sine_bb * a) >> 9);
    } else {
        xPositions[0] = XPos + ((sine_bb * (-pivotY - 2) + cosine_bb * (-pivotX - 2)) >> 9);
        yPositions[0] = YPos + ((cosine_bb * (-pivotY - 2) - sine_bb * (-pivotX - 2)) >> 9);
        xPositions[1] = XPos + ((sine_bb * (-pivotY - 2) + cosine_bb * (width - pivotX + 2)) >> 9);
        yPositions[1] = YPos + ((cosine_bb * (-pivotY - 2) - sine_bb * (width - pivotX + 2)) >> 9);
        xPositions[2] = XPos + ((sine_bb * (height - pivotY + 2) + cosine_bb * (-pivotX - 2)) >> 9);
        yPositions[2] = YPos + ((cosine_bb * (height - pivotY + 2) - sine_bb * (-pivotX - 2)) >> 9);
        int a = width - pivotX + 2; int b = height - pivotY + 2;
        xPositions[3] = XPos + ((sine_bb * b + cosine_bb * a) >> 9);
        yPositions[3] = YPos + ((cosine_bb * b - sine_bb * a) >> 9);
    }
    // --- FIN BOUNDING BOX ---

    // `truescale` es el inverso del factor de escala para el paso de textura
    int truescale_inv = (scale != 0) ? (int)((512.0f / (float)scale) * 512.0f) : 0;
    if(truescale_inv == 0 && scale !=0) truescale_inv =1;
    
    // Renombrar para claridad: estos son los pasos de textura por píxel de pantalla
    long long sine_step   = (long long)truescale_inv * sin512LookupTable[angle] >> 9;
    long long cosine_step = (long long)truescale_inv * cos512LookupTable[angle] >> 9;

    int bb_left = GFX_LINESIZE; for(int i=0;i<4;++i)if(xPositions[i]<bb_left)bb_left=xPositions[i]; if(bb_left<0)bb_left=0;
    int bb_right = 0; for(int i=0;i<4;++i)if(xPositions[i]>bb_right)bb_right=xPositions[i]; if(bb_right>GFX_LINESIZE)bb_right=GFX_LINESIZE;
    int maxX = bb_right - bb_left;
    int bb_top = SCREEN_YSIZE; for(int i=0;i<4;++i)if(yPositions[i]<bb_top)bb_top=yPositions[i]; if(bb_top<0)bb_top=0;
    int bb_bottom = 0; for(int i=0;i<4;++i)if(yPositions[i]>bb_bottom)bb_bottom=yPositions[i]; if(bb_bottom>SCREEN_YSIZE)bb_bottom=SCREEN_YSIZE;
    int maxY = bb_bottom - bb_top;
    if (maxX <= 0 || maxY <= 0) return;

    int fb_pitch = GFX_LINESIZE - maxX;
    int spritesheet_pitch_shift = surface->widthShift; // Tu 'lineSize'
    int spritesheet_actual_width = surface->width;

    ushort *frameBufferPtr_line = &Engine.frameBuffer[bb_left + GFX_LINESIZE * bb_top];
    byte *lineBuffer_ptr = &gfxLineBuffer[bb_top];
    int startX_screen_rel = bb_left - XPos;
    int startY_screen_rel = bb_top - YPos;
    
    // Ajuste heurístico de RSDK (sprYPos es el pivote Y en textura)
    if (cosine_step < 0 || sine_step < 0) sprYPos += sine_step + cosine_step;

    long long drawX, drawY; // Coordenadas de textura iniciales para la primera línea
    if (direction == FLIP_X) {
        drawX = sprXPos - (cosine_step * startX_screen_rel - sine_step * startY_screen_rel) - (truescale_inv >> 1);
        drawY = cosine_step * startY_screen_rel + sprYPos + sine_step * startX_screen_rel; 
    } else {
        drawX = sprXPos + (cosine_step * startX_screen_rel - sine_step * startY_screen_rel);
        drawY = cosine_step * startY_screen_rel + sprYPos + sine_step * startX_screen_rel;
    }

    for (int y_loop = 0; y_loop < maxY; ++y_loop) { // Tu `while (maxY--)`
        if (lineBuffer_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*lineBuffer_ptr];
        lineBuffer_ptr++;
        long long finalX = drawX; 
        long long finalY = drawY; 
        ushort *fbPxl = frameBufferPtr_line;
        for (int x_loop = 0; x_loop < maxX; ++x_loop) { // Tu `while (w--)`
            if (finalX > shiftPivot_fixed && finalX < fullwidth_fixed && 
                finalY > shiftheight_fixed && finalY < fullheight_fixed) {
                // Usar surface->width para el pitch de la textura si widthShift no es fiable
                int tex_idx = ( (int)(finalY >> 9) * spritesheet_actual_width ) + (int)(finalX >> 9);
                // Tu original: gfxData[(finalY >> 9 << spritesheet_line_pitch_shift) + (finalX >> 9)];
                if (tex_idx >= 0 && tex_idx < (spritesheet_actual_width * surface->height)) { // Seguridad
                    byte sprite_color_idx = gfxData[tex_idx];
                    if (sprite_color_idx > 0) {
                        *fbPxl = activePalette[sprite_color_idx];
                    }
                }
            }
            fbPxl++;
            if (direction == FLIP_X) { finalX -= cosine_step; finalY += sine_step; }
            else { finalX += cosine_step; finalY += sine_step; }
        }
        if (direction == FLIP_X) { drawX += sine_step; drawY += cosine_step; }
        else { drawX -= sine_step; drawY += cosine_step; }
        frameBufferPtr_line += GFX_LINESIZE; 
    }
#endif 
}

void DrawSpriteAllFX(int direction_param, int XPos_screen, int YPos_screen, 
                     int pivotX_sprite, int pivotY_sprite, 
                     int sprX_in_sheet, int sprY_in_sheet, 
                     int frame_width, int frame_height, 
                     int rotation_angle_param, int scale_factor_param, 
                     int sheetID, 
                     int alpha_param, int ink_effect_param, int flags)
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;
    GFXSurface *surface = &gfxSurface[sheetID]; 
    if (surface->dataPosition >= GFXDATA_SIZE) return; 
    byte *gfxData = &graphicData[surface->dataPosition];

    int hscale = scale_factor_param;
    int vscale = scale_factor_param;
    int ink = ink_effect_param;
    int alpha = alpha_param;
    int rotation = rotation_angle_param;
    int direction = direction_param;

    if ((flags & FX_INK) == 0) ink = INK_NONE;
    if ((flags & FX_HSCALE) == 0) hscale = 0x200;
    if ((flags & FX_VSCALE) == 0) vscale = 0x200;
    if ((flags & FX_ROTATE) == 0) rotation = 0;
    if ((flags & 3) == 0 && flags != 0) direction = FLIP_NONE; // Tu lógica original para flags de flip
    // Considera usar los defines FX_FLIPX, FX_FLIPY para mayor claridad si es posible:
    // else if (flags & FX_FLIPX) direction = (flags & FX_FLIPY) ? FLIP_XY : FLIP_X;
    // else if (flags & FX_FLIPY) direction = FLIP_Y;
	
	if (alpha < 0) alpha = 0; if (alpha > 0xFF) alpha = 0xFF;
    if (hscale == 0 || vscale == 0) return;
    if (ink != INK_NONE && ink != INK_BLEND && alpha == 0) return;

    int sprXPos    = (pivotX_sprite + sprX_in_sheet) << 9;
    int sprYPos    = (pivotY_sprite + sprY_in_sheet) << 9;
    // Usar los parámetros frame_width y frame_height para los límites de textura
    long long fullwidth_fixed  = (long long)(sprX_in_sheet + frame_width) << 9;
    long long fullheight_fixed = (long long)(sprY_in_sheet + frame_height) << 9;
    long long shiftPivot_fixed   = ((long long)sprX_in_sheet << 9) - 1;
    long long shiftheight_fixed  = ((long long)sprY_in_sheet << 9) - 1;

    int angle_calc = rotation & 0x1FF;
    if (angle_calc != 0) angle_calc = 0x200 - angle_calc;
    
    // Variables para el bounding box (tu código original)
    int sine_bb   = hscale * sin512LookupTable[angle_calc] >> 9;
    int cosine_bb = hscale * cos512LookupTable[angle_calc] >> 9;
    int vsine_bb  = vscale * sin512LookupTable[angle_calc] >> 9;
    int vcosine_bb= vscale * cos512LookupTable[angle_calc] >> 9;
    int xPositions[4], yPositions[4];

    // --- TU CÁLCULO ORIGINAL DEL BOUNDING BOX --- 
    if (direction == FLIP_X) {
        xPositions[0] = XPos_screen + ((vsine_bb * (-pivotY_sprite - 2) + cosine_bb * (pivotX_sprite + 2)) >> 9);
        yPositions[0] = YPos_screen + ((vcosine_bb * (-pivotY_sprite - 2) - sine_bb * (pivotX_sprite + 2)) >> 9);
        xPositions[1] = XPos_screen + ((vsine_bb * (-pivotY_sprite - 2) + cosine_bb * (pivotX_sprite - frame_width - 2)) >> 9);
        yPositions[1] = YPos_screen + ((vcosine_bb * (-pivotY_sprite - 2) - sine_bb * (pivotX_sprite - frame_width - 2)) >> 9);
        xPositions[2] = XPos_screen + ((vsine_bb * (frame_height - pivotY_sprite + 2) + cosine_bb * (pivotX_sprite + 2)) >> 9);
        yPositions[2] = YPos_screen + ((vcosine_bb * (frame_height - pivotY_sprite + 2) - sine_bb * (pivotX_sprite + 2)) >> 9);
        int a = pivotX_sprite - frame_width - 2; int b = frame_height - pivotY_sprite + 2;
        xPositions[3] = XPos_screen + ((vsine_bb * b + cosine_bb * a) >> 9);
        yPositions[3] = YPos_screen + ((vcosine_bb * b - sine_bb * a) >> 9);
    } else if (direction == FLIP_Y) {
        xPositions[0] = XPos_screen + ((vsine_bb * (pivotY_sprite + 2) + cosine_bb * (-pivotX_sprite - 2)) >> 9);
        yPositions[0] = YPos_screen + ((vcosine_bb * (pivotY_sprite + 2) - sine_bb * (-pivotX_sprite - 2)) >> 9);
        xPositions[1] = XPos_screen + ((vsine_bb * (pivotY_sprite + 2) + cosine_bb * (frame_width - pivotX_sprite + 2)) >> 9);
        yPositions[1] = YPos_screen + ((vcosine_bb * (pivotY_sprite + 2) - sine_bb * (frame_width - pivotX_sprite + 2)) >> 9);
        xPositions[2] = XPos_screen + ((vsine_bb * (pivotY_sprite - frame_height - 2) + cosine_bb * (-pivotX_sprite - 2)) >> 9);
        yPositions[2] = YPos_screen + ((vcosine_bb * (pivotY_sprite - frame_height - 2) - sine_bb * (-pivotX_sprite - 2)) >> 9);
        int a = frame_width - pivotX_sprite + 2; int b = pivotY_sprite - frame_height - 2;
        xPositions[3] = XPos_screen + ((vsine_bb * b + cosine_bb * a) >> 9);
        yPositions[3] = YPos_screen + ((vcosine_bb * b - sine_bb * a) >> 9);
    } else if (direction == FLIP_XY) {
        xPositions[0] = XPos_screen + ((vsine_bb * (pivotY_sprite + 2) + cosine_bb * (pivotX_sprite + 2)) >> 9);
        yPositions[0] = YPos_screen + ((vcosine_bb * (pivotY_sprite + 2) - sine_bb * (pivotX_sprite + 2)) >> 9);
        xPositions[1] = XPos_screen + ((vsine_bb * (pivotY_sprite + 2) + cosine_bb * (pivotX_sprite - frame_width - 2)) >> 9);
        yPositions[1] = YPos_screen + ((vcosine_bb * (pivotY_sprite + 2) - sine_bb * (pivotX_sprite - frame_width - 2)) >> 9);
        xPositions[2] = XPos_screen + ((vsine_bb * (pivotY_sprite - frame_height - 2) + cosine_bb * (pivotX_sprite + 2)) >> 9);
        yPositions[2] = YPos_screen + ((vcosine_bb * (pivotY_sprite - frame_height - 2) - sine_bb * (pivotX_sprite + 2)) >> 9);
        int a = pivotX_sprite - frame_width - 2; int b = pivotY_sprite - frame_height - 2;
        xPositions[3] = XPos_screen + ((vsine_bb * b + cosine_bb * a) >> 9);
        yPositions[3] = YPos_screen + ((vcosine_bb * b - sine_bb * a) >> 9);
    } else { // FLIP_NONE
        xPositions[0] = XPos_screen + ((vsine_bb * (-pivotY_sprite - 2) + cosine_bb * (-pivotX_sprite - 2)) >> 9);
        yPositions[0] = YPos_screen + ((vcosine_bb * (-pivotY_sprite - 2) - sine_bb * (-pivotX_sprite - 2)) >> 9);
        xPositions[1] = XPos_screen + ((vsine_bb * (-pivotY_sprite - 2) + cosine_bb * (frame_width - pivotX_sprite + 2)) >> 9);
        yPositions[1] = YPos_screen + ((vcosine_bb * (-pivotY_sprite - 2) - sine_bb * (frame_width - pivotX_sprite + 2)) >> 9);
        xPositions[2] = XPos_screen + ((vsine_bb * (frame_height - pivotY_sprite + 2) + cosine_bb * (-pivotX_sprite - 2)) >> 9);
        yPositions[2] = YPos_screen + ((vcosine_bb * (frame_height - pivotY_sprite + 2) - sine_bb * (-pivotX_sprite - 2)) >> 9);
        int a = frame_width - pivotX_sprite + 2; int b = frame_height - pivotY_sprite + 2;
        xPositions[3] = XPos_screen + ((vsine_bb * b + cosine_bb * a) >> 9);
        yPositions[3] = YPos_screen + ((vcosine_bb * b - sine_bb * a) >> 9);
    }
    // --- FIN BOUNDING BOX ---

    int truescale_h_inv_loc = (hscale != 0) ? (int)((512.0f / (float)hscale) * 512.0f) : 1; // Tu 'truescale' para h
    long long cosine_step = (long long)truescale_h_inv_loc * cos512LookupTable[angle_calc] >> 9; // Tu 'cosine' para barrido
    long long sine_step   = (long long)truescale_h_inv_loc * sin512LookupTable[angle_calc] >> 9; // Tu 'sine' para barrido
    
    int truescale_v_inv_loc = (vscale != 0) ? (int)((512.0f / (float)vscale) * 512.0f) : 1; // Tu 'truescale' para v
    long long vcosine_step  = (long long)truescale_v_inv_loc * cos512LookupTable[angle_calc] >> 9; // Tu 'vcosine' para barrido
    long long vsine_step    = (long long)truescale_v_inv_loc * sin512LookupTable[angle_calc] >> 9; // Tu 'vsine' para barrido

    if(truescale_h_inv_loc == 0 && hscale !=0) truescale_h_inv_loc =1;
    if(truescale_v_inv_loc == 0 && vscale !=0) truescale_v_inv_loc =1;

    int bb_left=GFX_LINESIZE; for(int i=0;i<4;++i)if(xPositions[i]<bb_left)bb_left=xPositions[i]; if(bb_left<0)bb_left=0;
    int bb_right=0; for(int i=0;i<4;++i)if(xPositions[i]>bb_right)bb_right=xPositions[i]; if(bb_right>GFX_LINESIZE)bb_right=GFX_LINESIZE;
    int maxX = bb_right-bb_left;
    int bb_top=SCREEN_YSIZE; for(int i=0;i<4;++i)if(yPositions[i]<bb_top)bb_top=yPositions[i]; if(bb_top<0)bb_top=0;
    int bb_bottom=0; for(int i=0;i<4;++i)if(yPositions[i]>bb_bottom)bb_bottom=yPositions[i]; if(bb_bottom>SCREEN_YSIZE)bb_bottom=SCREEN_YSIZE;
    int maxY = bb_bottom-bb_top;
    if (maxX <= 0 || maxY <= 0) return;

    int fb_pitch = GFX_LINESIZE - maxX;
    int spritesheet_line_pitch_shift = surface->widthShift; // Tu 'lineSize'

    ushort *frameBufferPtr_line = &Engine.frameBuffer[bb_left + GFX_LINESIZE * bb_top];
    byte *lineBuffer_ptr = &gfxLineBuffer[bb_top];
    int startX_screen_rel = bb_left - XPos_screen;
    int startY_screen_rel = bb_top - YPos_screen;
    
    if (vcosine_step < 0 || vsine_step < 0) sprYPos += vsine_step + vcosine_step;

    long long drawX, drawY;
    // --- TU LÓGICA ORIGINAL PARA INICIALIZAR drawX, drawY --- 
    if (direction == FLIP_X) {
        drawX = sprXPos - (cosine_step * startX_screen_rel - sine_step * startY_screen_rel) - (truescale_h_inv_loc >> 1);
        drawY = vcosine_step * startY_screen_rel + sprYPos + vsine_step * startX_screen_rel;
    } else if (direction == FLIP_Y) {
        drawX = sprXPos + cosine_step * startX_screen_rel - sine_step * startY_screen_rel;
        drawY = sprYPos - (vcosine_step * startY_screen_rel + vsine_step * startX_screen_rel);
    } else if (direction == FLIP_XY) {
        drawX = sprXPos - (cosine_step * startX_screen_rel - sine_step * startY_screen_rel) - (truescale_h_inv_loc >> 1);
        drawY = sprYPos - (vcosine_step * startY_screen_rel + vsine_step * startX_screen_rel);
    } else { // FLIP_NONE
        drawX = sprXPos + cosine_step * startX_screen_rel - sine_step * startY_screen_rel;
        drawY = vcosine_step * startY_screen_rel + sprYPos + vsine_step * startX_screen_rel;
    }
    // --- FIN INICIALIZACIÓN drawX, drawY ---

    for (int y_loop = 0; y_loop < maxY; ++y_loop) {
        if (lineBuffer_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*lineBuffer_ptr];
        lineBuffer_ptr++;
        long long finalX = drawX, finalY = drawY;
        ushort *fbPxl = frameBufferPtr_line;
        for (int x_loop = 0; x_loop < maxX; ++x_loop) {
            if (finalX > shiftPivot_fixed && finalX < fullwidth_fixed && finalY > shiftheight_fixed && finalY < fullheight_fixed) {
                int tex_x_i = (int)(finalX >> 9), tex_y_i = (int)(finalY >> 9);
                int spr_idx = (tex_y_i << spritesheet_line_pitch_shift) + tex_x_i;
                if (spr_idx >= 0 && spr_idx < (surface->width * surface->height)) {
                    byte spr_color_idx = gfxData[spr_idx];
                    if (spr_color_idx > 0) {
                        ushort src_color = activePalette[spr_color_idx];
                        if (ink == INK_NONE || (alpha == 0 && ink != INK_BLEND)) {
                            *fbPxl = src_color;
                        } else {
                            ushort dst_color = *fbPxl;
                            int sr=(src_color&0xF800)>>11, sg=(src_color&0x07E0)>>5, sb=(src_color&0x001F);
                            int dr=(dst_color&0xF800)>>11, dg=(dst_color&0x07E0)>>5, db=(dst_color&0x001F);
                            int R_res=dr, G_res=dg, B_res=db;
                            switch(ink) { 
                                case INK_BLEND: R_res=(sr+dr)>>1; G_res=(sg+dg)>>1; B_res=(sb+db)>>1; break;
                                case INK_ALPHA: R_res=((dr*(0xFF-alpha))+(sr*alpha))>>8; G_res=((dg*(0xFF-alpha))+(sg*alpha))>>8; B_res=((db*(0xFF-alpha))+(sb*alpha))>>8; break;
                                case INK_ADD: R_res=dr+((sr*alpha)>>8); if(R_res>31)R_res=31; G_res=dg+((sg*alpha)>>8); if(G_res>63)G_res=63; B_res=db+((sb*alpha)>>8); if(B_res>31)B_res=31; break;
                                case INK_SUB: R_res=dr-(((31-sr)*alpha)>>8); if(R_res<0)R_res=0; G_res=dg-(((63-sg)*alpha)>>8); if(G_res<0)G_res=0; B_res=db-(((31-sb)*alpha)>>8); if(B_res<0)B_res=0; break;
                            }
                            if(R_res<0)R_res=0; else if(R_res>31)R_res=31;
                            if(G_res<0)G_res=0; else if(G_res>63)G_res=63;
                            if(B_res<0)B_res=0; else if(B_res>31)B_res=31;
                            *fbPxl = (ushort)((R_res << 11) | (G_res << 5) | B_res);
                        }
                    }
                }
            }
            fbPxl++;
            // --- TU LÓGICA ORIGINAL PARA AVANZAR finalX, finalY --- 
            if (direction == FLIP_X || direction == FLIP_XY) { finalX -= cosine_step; finalY += vsine_step; } 
            else { finalX += cosine_step; finalY += vsine_step; }
            // --- FIN AVANCE finalX, finalY --- 
        }
        frameBufferPtr_line += fb_pitch; 
        // --- TU LÓGICA ORIGINAL PARA AVANZAR drawX, drawY para la siguiente línea --- 
        if (direction == FLIP_X || direction == FLIP_XY) { drawX += sine_step; drawY += vcosine_step; } 
        else { drawX -= sine_step; drawY += vcosine_step; }
        // --- FIN AVANCE drawX, drawY --- 
    }
#endif 
}

void DrawBlendedSprite(int XPos_screen, int YPos_screen, 
                       int frame_width, int frame_height, 
                       int sprX_in_sheet, int sprY_in_sheet, 
                       int sheetID)
{
#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;

    int current_XPos_screen = XPos_screen;
    int current_YPos_screen = YPos_screen;
    int current_frame_width = frame_width;
    int current_frame_height = frame_height;
    int current_sprX_in_sheet = sprX_in_sheet;
    int current_sprY_in_sheet = sprY_in_sheet;

    // --- Clipping ---
    if (current_XPos_screen < 0) { current_sprX_in_sheet -= current_XPos_screen; current_frame_width += current_XPos_screen; current_XPos_screen = 0; }
    if (current_YPos_screen < 0) { current_sprY_in_sheet -= current_YPos_screen; current_frame_height += current_YPos_screen; current_YPos_screen = 0; }
    if (current_XPos_screen + current_frame_width > GFX_LINESIZE) current_frame_width = GFX_LINESIZE - current_XPos_screen;
    if (current_YPos_screen + current_frame_height > SCREEN_YSIZE) current_frame_height = SCREEN_YSIZE - current_YPos_screen;
    if (current_frame_width <= 0 || current_frame_height <= 0) return;
    // --- Fin Clipping ---

    GFXSurface *surface_ptr = &gfxSurface[sheetID];
    if (surface_ptr->dataPosition >= GFXDATA_SIZE) return;
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];

    int framebuffer_pitch = GFX_LINESIZE - current_frame_width;
    int spritesheet_pitch = surface_ptr->width - current_frame_width;
    byte *line_palette_idx_ptr = &gfxLineBuffer[current_YPos_screen];
    byte *sprite_pixel_ptr = &sprite_pixel_data_origin[current_sprX_in_sheet + surface_ptr->width * current_sprY_in_sheet];
    ushort *screen_pixel_ptr = &Engine.frameBuffer[current_XPos_screen + GFX_LINESIZE * current_YPos_screen];

    for (int y_loop = 0; y_loop < current_frame_height; ++y_loop) {
        if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
        line_palette_idx_ptr++;

        for (int x_loop = 0; x_loop < current_frame_width; ++x_loop) {
            byte sprite_pixel_color_index = *sprite_pixel_ptr;
            if (sprite_pixel_color_index > 0) { 
                ushort sprite_color = activePalette[sprite_pixel_color_index];
                ushort framebuffer_color = *screen_pixel_ptr;

                // Blend 50/50 explícito por componentes para RGB565
                int r1 = (sprite_color & 0xF800) >> 11; int g1 = (sprite_color & 0x07E0) >> 5; int b1 = (sprite_color & 0x001F);
                int r2 = (framebuffer_color & 0xF800) >> 11; int g2 = (framebuffer_color & 0x07E0) >> 5; int b2 = (framebuffer_color & 0x001F);
                
                int r_avg = (r1 + r2) >> 1;
                int g_avg = (g1 + g2) >> 1; // g1, g2 son 0-63. g_avg es 0-63.
                int b_avg = (b1 + b2) >> 1;
                
                *screen_pixel_ptr = (ushort)((r_avg << 11) | (g_avg << 5) | b_avg);
            }
            sprite_pixel_ptr++;
            screen_pixel_ptr++;
        }
        screen_pixel_ptr += framebuffer_pitch;
        sprite_pixel_ptr += spritesheet_pitch;
    }
#endif 
}

void DrawAlphaBlendedSprite(int XPos_screen, int YPos_screen, 
                            int frame_width, int frame_height, 
                            int sprX_in_sheet, int sprY_in_sheet, 
                            int alpha_param, 
                            int sheetID)
{
    if (alpha_param > 0xFF) alpha_param = 0xFF;
    if (alpha_param < 0) alpha_param = 0;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;

    int current_XPos_screen = XPos_screen;
    int current_YPos_screen = YPos_screen;
    int current_frame_width = frame_width;
    int current_frame_height = frame_height;
    int current_sprX_in_sheet = sprX_in_sheet;
    int current_sprY_in_sheet = sprY_in_sheet;

    // --- Clipping ---
    if (current_XPos_screen < 0) { current_sprX_in_sheet -= current_XPos_screen; current_frame_width += current_XPos_screen; current_XPos_screen = 0; }
    if (current_YPos_screen < 0) { current_sprY_in_sheet -= current_YPos_screen; current_frame_height += current_YPos_screen; current_YPos_screen = 0; }
    if (current_XPos_screen + current_frame_width > GFX_LINESIZE) current_frame_width = GFX_LINESIZE - current_XPos_screen;
    if (current_YPos_screen + current_frame_height > SCREEN_YSIZE) current_frame_height = SCREEN_YSIZE - current_YPos_screen;
    if (current_frame_width <= 0 || current_frame_height <= 0 || alpha_param == 0) return;
    // --- Fin Clipping ---

    GFXSurface *surface_ptr = &gfxSurface[sheetID];
    if (surface_ptr->dataPosition >= GFXDATA_SIZE) return;
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];

    int framebuffer_pitch = GFX_LINESIZE - current_frame_width;
    int spritesheet_pitch = surface_ptr->width - current_frame_width;
    byte *line_palette_idx_ptr = &gfxLineBuffer[current_YPos_screen];
    byte *sprite_pixel_ptr = &sprite_pixel_data_origin[current_sprX_in_sheet + surface_ptr->width * current_sprY_in_sheet];
    ushort *screen_pixel_ptr = &Engine.frameBuffer[current_XPos_screen + GFX_LINESIZE * current_YPos_screen];

    if (alpha_param == 0xFF) { // Opaco
        for (int y = 0; y < current_frame_height; ++y) {
            if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
            line_palette_idx_ptr++;
            byte *sprite_px_line = sprite_pixel_ptr;
            ushort *screen_px_line = screen_pixel_ptr;
            for (int x = 0; x < current_frame_width; ++x) {
                if (*sprite_px_line > 0) *screen_px_line = activePalette[*sprite_px_line];
                sprite_px_line++; screen_px_line++;
            }
            screen_pixel_ptr += GFX_LINESIZE; sprite_pixel_ptr += surface_ptr->width;
        }
    } else { // Blending con Alpha
        int inv_alpha = 0xFF - alpha_param;
        for (int y = 0; y < current_frame_height; ++y) {
            if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
            line_palette_idx_ptr++;
            byte *sprite_px_line = sprite_pixel_ptr;
            ushort *screen_px_line = screen_pixel_ptr;
            for (int x = 0; x < current_frame_width; ++x) {
                byte sprite_color_idx = *sprite_px_line;
                if (sprite_color_idx > 0) {
                    ushort sprite_color_rgb565 = activePalette[sprite_color_idx];
                    ushort framebuffer_color_rgb565 = *screen_px_line;
                    int sr = (sprite_color_rgb565 & 0xF800) >> 11; int sg = (sprite_color_rgb565 & 0x07E0) >> 5; int sb = (sprite_color_rgb565 & 0x001F);
                    int dr = (framebuffer_color_rgb565 & 0xF800) >> 11; int dg = (framebuffer_color_rgb565 & 0x07E0) >> 5; int db = (framebuffer_color_rgb565 & 0x001F);
                    int final_r = ((dr * inv_alpha) + (sr * alpha_param)) >> 8;
                    int final_g = ((dg * inv_alpha) + (sg * alpha_param)) >> 8;
                    int final_b = ((db * inv_alpha) + (sb * alpha_param)) >> 8;
                    *screen_px_line = (ushort)((final_r << 11) | (final_g << 5) | final_b);
                }
                sprite_px_line++; screen_px_line++;
            }
            screen_pixel_ptr += GFX_LINESIZE; sprite_pixel_ptr += surface_ptr->width;
        }
    }
#endif 
}

void DrawAdditiveBlendedSprite(int XPos_screen, int YPos_screen, 
                               int frame_width, int frame_height, 
                               int sprX_in_sheet, int sprY_in_sheet, 
                               int alpha_param, 
                               int sheetID)
{
    if (alpha_param > 0xFF) alpha_param = 0xFF;
    if (alpha_param < 0) alpha_param = 0;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;

    int current_XPos_screen = XPos_screen;
    int current_YPos_screen = YPos_screen;
    int current_frame_width = frame_width;
    int current_frame_height = frame_height;
    int current_sprX_in_sheet = sprX_in_sheet;
    int current_sprY_in_sheet = sprY_in_sheet;

    // --- Clipping ---
    if (current_XPos_screen < 0) { current_sprX_in_sheet -= current_XPos_screen; current_frame_width += current_XPos_screen; current_XPos_screen = 0; }
    if (current_YPos_screen < 0) { current_sprY_in_sheet -= current_YPos_screen; current_frame_height += current_YPos_screen; current_YPos_screen = 0; }
    if (current_XPos_screen + current_frame_width > GFX_LINESIZE) current_frame_width = GFX_LINESIZE - current_XPos_screen;
    if (current_YPos_screen + current_frame_height > SCREEN_YSIZE) current_frame_height = SCREEN_YSIZE - current_YPos_screen;
    if (current_frame_width <= 0 || current_frame_height <= 0 || alpha_param == 0) return;
    // --- Fin Clipping ---

    GFXSurface *surface_ptr = &gfxSurface[sheetID];
    if (surface_ptr->dataPosition >= GFXDATA_SIZE) return;
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];

    int framebuffer_pitch = GFX_LINESIZE - current_frame_width;
    int spritesheet_pitch = surface_ptr->width - current_frame_width;
    byte *line_palette_idx_ptr = &gfxLineBuffer[current_YPos_screen];
    byte *sprite_pixel_ptr = &sprite_pixel_data_origin[current_sprX_in_sheet + surface_ptr->width * current_sprY_in_sheet];
    ushort *screen_pixel_ptr = &Engine.frameBuffer[current_XPos_screen + GFX_LINESIZE * current_YPos_screen];

    for (int y_loop = 0; y_loop < current_frame_height; ++y_loop) {
        if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
        line_palette_idx_ptr++;
        byte *sprite_px_line = sprite_pixel_ptr;
        ushort *screen_px_line = screen_pixel_ptr;

        for (int x_loop = 0; x_loop < current_frame_width; ++x_loop) {
            byte sprite_color_idx = *sprite_px_line;
            if (sprite_color_idx > 0) { 
                ushort sprite_color_rgb565 = activePalette[sprite_color_idx];
                ushort framebuffer_color_rgb565 = *screen_px_line;

                int sr = (sprite_color_rgb565 & 0xF800) >> 11; int sg = (sprite_color_rgb565 & 0x07E0) >> 5; int sb = (sprite_color_rgb565 & 0x001F);
                int dr = (framebuffer_color_rgb565 & 0xF800) >> 11; int dg = (framebuffer_color_rgb565 & 0x07E0) >> 5; int db = (framebuffer_color_rgb565 & 0x001F);

                int final_r = dr + ((sr * alpha_param) >> 8); if (final_r > 31) final_r = 31;
                int final_g = dg + ((sg * alpha_param) >> 8); if (final_g > 63) final_g = 63;
                int final_b = db + ((sb * alpha_param) >> 8); if (final_b > 31) final_b = 31;
                
                *screen_px_line = (ushort)((final_r << 11) | (final_g << 5) | final_b);
            }
            sprite_px_line++;
            screen_px_line++;
        }
        screen_pixel_ptr += GFX_LINESIZE; 
        sprite_pixel_ptr += surface_ptr->width; 
    }
#endif 
}

void DrawSubtractiveBlendedSprite(int XPos_screen, int YPos_screen, 
                                  int frame_width, int frame_height, 
                                  int sprX_in_sheet, int sprY_in_sheet, 
                                  int alpha_param, 
                                  int sheetID)
{
    if (alpha_param > 0xFF) alpha_param = 0xFF;
    if (alpha_param < 0) alpha_param = 0;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;

    int current_XPos_screen = XPos_screen;
    int current_YPos_screen = YPos_screen;
    int current_frame_width = frame_width;
    int current_frame_height = frame_height;
    int current_sprX_in_sheet = sprX_in_sheet;
    int current_sprY_in_sheet = sprY_in_sheet;

    // --- Clipping ---
    if (current_XPos_screen < 0) { current_sprX_in_sheet -= current_XPos_screen; current_frame_width += current_XPos_screen; current_XPos_screen = 0; }
    if (current_YPos_screen < 0) { current_sprY_in_sheet -= current_YPos_screen; current_frame_height += current_YPos_screen; current_YPos_screen = 0; }
    if (current_XPos_screen + current_frame_width > GFX_LINESIZE) current_frame_width = GFX_LINESIZE - current_XPos_screen;
    if (current_YPos_screen + current_frame_height > SCREEN_YSIZE) current_frame_height = SCREEN_YSIZE - current_YPos_screen;
    if (current_frame_width <= 0 || current_frame_height <= 0 || alpha_param == 0) return;
    // --- Fin Clipping ---

    GFXSurface *surface_ptr = &gfxSurface[sheetID];
    if (surface_ptr->dataPosition >= GFXDATA_SIZE) return;
    byte *sprite_pixel_data_origin = &graphicData[surface_ptr->dataPosition];

    int framebuffer_pitch = GFX_LINESIZE - current_frame_width;
    int spritesheet_pitch = surface_ptr->width - current_frame_width;
    byte *line_palette_idx_ptr = &gfxLineBuffer[current_YPos_screen];
    byte *sprite_pixel_ptr = &sprite_pixel_data_origin[current_sprX_in_sheet + surface_ptr->width * current_sprY_in_sheet];
    ushort *screen_pixel_ptr = &Engine.frameBuffer[current_XPos_screen + GFX_LINESIZE * current_YPos_screen];

    for (int y_loop = 0; y_loop < current_frame_height; ++y_loop) {
        if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
        line_palette_idx_ptr++;
        byte *sprite_px_line = sprite_pixel_ptr;
        ushort *screen_px_line = screen_pixel_ptr;

        for (int x_loop = 0; x_loop < current_frame_width; ++x_loop) {
            byte sprite_color_idx = *sprite_px_line;
            if (sprite_color_idx > 0) { 
                ushort sprite_color_rgb565 = activePalette[sprite_color_idx];
                ushort framebuffer_color_rgb565 = *screen_px_line;

                int sr = (sprite_color_rgb565 & 0xF800) >> 11; int sg = (sprite_color_rgb565 & 0x07E0) >> 5; int sb = (sprite_color_rgb565 & 0x001F);
                int dr = (framebuffer_color_rgb565 & 0xF800) >> 11; int dg = (framebuffer_color_rgb565 & 0x07E0) >> 5; int db = (framebuffer_color_rgb565 & 0x001F);

                int val_to_subtract_r = ((31 - sr) * alpha_param) >> 8;
                int val_to_subtract_g = ((63 - sg) * alpha_param) >> 8;
                int val_to_subtract_b = ((31 - sb) * alpha_param) >> 8;

                int final_r = dr - val_to_subtract_r; if (final_r < 0) final_r = 0;
                int final_g = dg - val_to_subtract_g; if (final_g < 0) final_g = 0;
                int final_b = db - val_to_subtract_b; if (final_b < 0) final_b = 0;
                
                *screen_px_line = (ushort)((final_r << 11) | (final_g << 5) | final_b);
            }
            sprite_px_line++;
            screen_px_line++;
        }
        screen_pixel_ptr += GFX_LINESIZE; 
        sprite_pixel_ptr += surface_ptr->width; 
    }
#endif 
}

void DrawObjectAnimation(void *objScr, void *ent, int XPos_screen, int YPos_screen) 
{
    #ifdef PS3_PPU_PRX_LOADER
    ObjectScript *temp_objScript_ptr = (ObjectScript *)objScr;
    Entity *temp_entity_ptr = (Entity *)ent;
    printf("PS3_DEBUG: DrawObjectAnimation(Ent: %p, Type: %d, Anim: %d, Frame: %d, X: %d, Y: %d) called.\n",
           (void*)temp_entity_ptr, 
           temp_entity_ptr ? temp_entity_ptr->type : -1, 
           temp_entity_ptr ? temp_entity_ptr->animation : -1, 
           temp_entity_ptr ? temp_entity_ptr->frame : -1, 
           XPos_screen, YPos_screen);
    if (temp_objScript_ptr && temp_objScript_ptr->animFile) {
        printf("             AnimFile: %p, aniListOffset: %d\n", 
               (void*)temp_objScript_ptr->animFile, temp_objScript_ptr->animFile->aniListOffset);
    } else if (temp_objScript_ptr) {
        printf("             AnimFile: NULL\n");
    }
    #endif
    if (!objScr || !ent) return;
    ObjectScript *objectScript_ptr = (ObjectScript *)objScr; 
    Entity *entity_ptr = (Entity *)ent;                     
    if (!objectScript_ptr->animFile) {
        // printf("PS3 DEBUG DrawObjectAnimation: animFile es NULL.\n");
        return;
    }

    int num_animations_in_file = objectScript_ptr->animFile->animCount; 

    if (entity_ptr->animation < 0 || entity_ptr->animation >= num_animations_in_file) {
        // printf("PS3 DEBUG DrawObjectAnimation: entity->animation %d fuera de rango (0 - %d).\n", entity_ptr->animation, num_animations_in_file - 1);
        return; 
    }

    SpriteAnimation *anim_ptr = &animationList[objectScript_ptr->animFile->aniListOffset + entity_ptr->animation]; 
    if (entity_ptr->frame < 0 || entity_ptr->frame >= anim_ptr->frameCount) {
        if (anim_ptr->frameCount == 0) return;
        return; 
    }
    SpriteFrame *current_frame_ptr = &animFrames[anim_ptr->frameListOffset + entity_ptr->frame]; 
    int rotation_to_pass = 0; 

    switch (anim_ptr->rotationStyle) {
        case ROTSTYLE_NONE:
            DrawSpriteAllFX(entity_ptr->direction, XPos_screen, YPos_screen, 
                            -current_frame_ptr->pivotX, -current_frame_ptr->pivotY, 
                            current_frame_ptr->sprX, current_frame_ptr->sprY, 
                            current_frame_ptr->width, current_frame_ptr->height,
							0, entity_ptr->scale, current_frame_ptr->sheetID, 
                            entity_ptr->alpha, entity_ptr->inkEffect, (FX_ALL & ~FX_ROTATE));
            break;
        case ROTSTYLE_FULL:
            DrawSpriteAllFX(entity_ptr->direction, XPos_screen, YPos_screen, 
                            -current_frame_ptr->pivotX, -current_frame_ptr->pivotY, 
                            current_frame_ptr->sprX, current_frame_ptr->sprY, 
                            current_frame_ptr->width, current_frame_ptr->height,
							entity_ptr->rotation, entity_ptr->scale, current_frame_ptr->sheetID, 
                            entity_ptr->alpha, entity_ptr->inkEffect, FX_ALL);
            break;
        case ROTSTYLE_45DEG:
            if (entity_ptr->rotation >= 0x100)
                rotation_to_pass = 0x200 - (((0x200 + 32 - 1) - entity_ptr->rotation + 20) >> 6 << 6); 
            else 
                rotation_to_pass = (entity_ptr->rotation + 32) >> 6 << 6; 
            DrawSpriteAllFX(entity_ptr->direction, XPos_screen, YPos_screen, 
                            -current_frame_ptr->pivotX, -current_frame_ptr->pivotY, 
                            current_frame_ptr->sprX, current_frame_ptr->sprY, 
                            current_frame_ptr->width, current_frame_ptr->height,
							rotation_to_pass, entity_ptr->scale, current_frame_ptr->sheetID, 
                            entity_ptr->alpha, entity_ptr->inkEffect, FX_ALL);
			break;
        case ROTSTYLE_STATICFRAMES: {
            int base_rotation_idx; 
            if (entity_ptr->rotation >= 0x100) {
                base_rotation_idx = 8 - ((0x214 - entity_ptr->rotation) >> 6);
            } else {
                base_rotation_idx = (entity_ptr->rotation + 20) >> 6;
            }
            base_rotation_idx &= 7; // Asegurar rango 0-7

            int frameID_to_use = entity_ptr->frame; 
            int display_rotation_angle = 0;  

            switch (base_rotation_idx) {
                case 0: 
                case 8: // Aunque &7 lo previene, por si acaso la lógica original lo consideraba.
                    display_rotation_angle = 0x000;
                    break;
                case 1: 
                    frameID_to_use += anim_ptr->frameCount;
                    display_rotation_angle = (entity_ptr->direction == FLIP_NONE) ? 0x080 : 0x000; 
                    break;
                case 2: 
                    display_rotation_angle = 0x080;
                    break;
                case 3: 
                    frameID_to_use += anim_ptr->frameCount;
                    display_rotation_angle = (entity_ptr->direction == FLIP_NONE) ? 0x100 : 0x080;
                    break;
                case 4: 
                    display_rotation_angle = 0x100;
                    break;
                case 5: 
                    frameID_to_use += anim_ptr->frameCount;
                    display_rotation_angle = (entity_ptr->direction == FLIP_NONE) ? 0x180 : 0x100;
                    break;
                case 6: 
                    display_rotation_angle = 0x180; // 384 en RSDK
                    break;
                case 7: 
                    frameID_to_use += anim_ptr->frameCount;
                    display_rotation_angle = (entity_ptr->direction == FLIP_NONE) ? 0x000 : 0x180;
                    break;
                default: 
                    display_rotation_angle = 0x000;
                    break;
            }

            // Comprobación de seguridad para frameID_to_use
            // MAX_SPRITE_FRAMES debe estar definido como el tamaño total del array animFrames.
            if (anim_ptr->frameListOffset + frameID_to_use >= MAX_SPRITE_FRAMES) { 
                 frameID_to_use = entity_ptr->frame; // Fallback al frame original
                 display_rotation_angle = 0;         // Sin rotación si el frame calculado es inválido
            }

            SpriteFrame *frame_to_draw = &animFrames[anim_ptr->frameListOffset + frameID_to_use];

            DrawSpriteAllFX(entity_ptr->direction, XPos_screen, YPos_screen, 
                            -frame_to_draw->pivotX, -frame_to_draw->pivotY, 
                            frame_to_draw->sprX, frame_to_draw->sprY, 
                            frame_to_draw->width, frame_to_draw->height,
							display_rotation_angle, 
                            entity_ptr->scale, frame_to_draw->sheetID, 
                            entity_ptr->alpha, entity_ptr->inkEffect, 
                            FX_ALL); 
            break;
        } // Fin case ROTSTYLE_STATICFRAMES
        default: break;
    }
}

void DrawFace(void *v, uint color_param)
{
    Vertex *verts = (Vertex *)v;
    if (!verts) return;

    int alpha_val = (color_param >> 24) & 0xFF; 
    if (alpha_val == 0) return; 

    // ... (Comprobaciones de culling básicas como en tu original) ...
    if (verts[0].x < 0 && verts[1].x < 0 && verts[2].x < 0 && verts[3].x < 0) return;
    if (verts[0].x >= GFX_LINESIZE && verts[1].x >= GFX_LINESIZE && verts[2].x >= GFX_LINESIZE && verts[3].x >= GFX_LINESIZE) return;
    if (verts[0].y < 0 && verts[1].y < 0 && verts[2].y < 0 && verts[3].y < 0) return;
    if (verts[0].y >= SCREEN_YSIZE && verts[1].y >= SCREEN_YSIZE && verts[2].y >= SCREEN_YSIZE && verts[3].y >= SCREEN_YSIZE) return;
    if (verts[0].x == verts[1].x && verts[1].x == verts[2].x && verts[2].x == verts[3].x) return;
    if (verts[0].y == verts[1].y && verts[1].y == verts[2].y && verts[2].y == verts[3].y) return;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;

    int v_idx[4] = {0, 1, 2, 3};
    for(int i=0; i<3; ++i) { /* ... Ordenar v_idx por verts[v_idx[j]].y ... */ 
        for(int j=0; j < 3-i; ++j) {
            if(verts[v_idx[j]].y > verts[v_idx[j+1]].y) { int temp_idx = v_idx[j]; v_idx[j] = v_idx[j+1]; v_idx[j+1] = temp_idx; }
        }
    }
    Vertex *vA = &verts[v_idx[0]], *vB = &verts[v_idx[1]], *vC = &verts[v_idx[2]], *vD = &verts[v_idx[3]];

    int poly_scan_top_y = vA->y, poly_scan_bottom_y = vD->y;
    if (poly_scan_top_y < 0) poly_scan_top_y = 0;
    if (poly_scan_bottom_y > SCREEN_YSIZE) poly_scan_bottom_y = SCREEN_YSIZE;
    if (poly_scan_top_y >= poly_scan_bottom_y) return;

    for (int i = poly_scan_top_y; i < poly_scan_bottom_y; ++i) { faceLineStart[i] = GFX_LINESIZE +1; faceLineEnd[i] = -1; }

    ProcessScanEdge(vA, vB); ProcessScanEdge(vA, vC); ProcessScanEdge(vA, vD);
    ProcessScanEdge(vB, vC); ProcessScanEdge(vB, vD); ProcessScanEdge(vC, vD);

    ushort poly_color_rgb565 = PACK_RGB888(((color_param >> 16) & 0xFF), ((color_param >> 8) & 0xFF), (color_param & 0xFF));
    ushort *current_fb_scanline_ptr = &Engine.frameBuffer[GFX_LINESIZE * poly_scan_top_y];

    if (alpha_val >= 255) { 
        for (int y_scan = poly_scan_top_y; y_scan < poly_scan_bottom_y; ++y_scan) {
            int start_x = faceLineStart[y_scan], end_x = faceLineEnd[y_scan];
            if (start_x < 0) start_x = 0; if (end_x > GFX_LINESIZE) end_x = GFX_LINESIZE;
            if (start_x < end_x) {
                ushort *pixel_write_ptr = &current_fb_scanline_ptr[start_x];
                for (int x_pixel = start_x; x_pixel < end_x; ++x_pixel) *pixel_write_ptr++ = poly_color_rgb565;
            }
            current_fb_scanline_ptr += GFX_LINESIZE;
        }
    } else { 
        ushort *fbufferBlend_table_ptr = &blendLookupTable[0x20 * (0xFF - alpha_val)];
        ushort *pixelBlend_table_ptr   = &blendLookupTable[0x20 * alpha_val];
        int poly_R_5 = (poly_color_rgb565 & 0xF800) >> 11, poly_G_6 = (poly_color_rgb565 & 0x07E0) >> 5, poly_B_5 = (poly_color_rgb565 & 0x001F);
        int poly_G_5_tbl = poly_G_6 >> 1;
        for (int y_scan = poly_scan_top_y; y_scan < poly_scan_bottom_y; ++y_scan) {
            int start_x = faceLineStart[y_scan], end_x = faceLineEnd[y_scan];
            if (start_x < 0) start_x = 0; if (end_x > GFX_LINESIZE) end_x = GFX_LINESIZE;
            if (start_x < end_x) {
                ushort *pixel_write_ptr = &current_fb_scanline_ptr[start_x];
                for (int x_pixel = start_x; x_pixel < end_x; ++x_pixel) {
                    ushort fb_px_val = *pixel_write_ptr;
                    int fb_R_5 = (fb_px_val & 0xF800)>>11, fb_G_6 = (fb_px_val & 0x07E0)>>5, fb_B_5 = (fb_px_val & 0x001F);
                    int fb_G_5_tbl = fb_G_6 >> 1;
                    int r = fbufferBlend_table_ptr[fb_R_5] + pixelBlend_table_ptr[poly_R_5];
                    int g = fbufferBlend_table_ptr[fb_G_5_tbl] + pixelBlend_table_ptr[poly_G_5_tbl];
                    int b = fbufferBlend_table_ptr[fb_B_5] + pixelBlend_table_ptr[poly_B_5];
                    if(r>31)r=31; if(g>31)g=31; if(b>31)b=31; int g6=(g<<1); if(g6>63)g6=63;
                    *pixel_write_ptr = (ushort)((r<<11)|(g6<<5)|b);
                    pixel_write_ptr++;
                }
            }
            current_fb_scanline_ptr += GFX_LINESIZE;
        }
    }
#endif 
}

void DrawFadedFace(void *v, uint base_color_param, uint fog_color_param, int alpha_param)
{
    Vertex *verts = (Vertex *)v;
    if (!verts) return;

    if (alpha_param > 0xFF) alpha_param = 0xFF;
    if (alpha_param < 0) alpha_param = 0; 
    // Si alpha_param es 0, se dibujará 100% fog_color_param con la lógica actual.
    // Si no se debe dibujar nada con alpha 0, la condición sería: if (alpha_param <= 0) return;

    // --- Comprobaciones de Culling (igual que DrawFace) ---
    if (verts[0].x < 0 && verts[1].x < 0 && verts[2].x < 0 && verts[3].x < 0) return;
    if (verts[0].x >= GFX_LINESIZE && verts[1].x >= GFX_LINESIZE && verts[2].x >= GFX_LINESIZE && verts[3].x >= GFX_LINESIZE) return;
    if (verts[0].y < 0 && verts[1].y < 0 && verts[2].y < 0 && verts[3].y < 0) return;
    if (verts[0].y >= SCREEN_YSIZE && verts[1].y >= SCREEN_YSIZE && verts[2].y >= SCREEN_YSIZE && verts[3].y >= SCREEN_YSIZE) return;
    if (verts[0].x == verts[1].x && verts[1].x == verts[2].x && verts[2].x == verts[3].x) return;
    if (verts[0].y == verts[1].y && verts[1].y == verts[2].y && verts[2].y == verts[3].y) return;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;

    // --- Ordenar Vértices y Calcular Scanlines (igual que DrawFace) ---
    int v_idx[4] = {0,1,2,3}; for(int i=0;i<3;++i)for(int j=0;j<3-i;++j)if(verts[v_idx[j]].y > verts[v_idx[j+1]].y){int t=v_idx[j];v_idx[j]=v_idx[j+1];v_idx[j+1]=t;}
    Vertex *vA=&verts[v_idx[0]], *vB=&verts[v_idx[1]], *vC=&verts[v_idx[2]], *vD=&verts[v_idx[3]];
    int poly_scan_top_y = vA->y, poly_scan_bottom_y = vD->y;
    if(poly_scan_top_y<0)poly_scan_top_y=0; if(poly_scan_bottom_y>SCREEN_YSIZE)poly_scan_bottom_y=SCREEN_YSIZE;
    if(poly_scan_top_y>=poly_scan_bottom_y)return;
    for(int i=poly_scan_top_y;i<poly_scan_bottom_y;++i){faceLineStart[i]=GFX_LINESIZE+1;faceLineEnd[i]=-1;}
    ProcessScanEdge(vA,vB); ProcessScanEdge(vA,vC); ProcessScanEdge(vA,vD);
    ProcessScanEdge(vB,vC); ProcessScanEdge(vB,vD); ProcessScanEdge(vC,vD);
    // --- Fin Ordenar Vértices ---

    ushort base_color_rgb565 = PACK_RGB888((base_color_param>>16)&0xFF, (base_color_param>>8)&0xFF, base_color_param&0xFF);
    ushort fog_color_rgb565  = PACK_RGB888((fog_color_param>>16)&0xFF, (fog_color_param>>8)&0xFF, fog_color_param&0xFF);

    ushort *fog_blend_ptr = &blendLookupTable[0x20 * (0xFF - alpha_param)];
    ushort *base_color_blend_ptr = &blendLookupTable[0x20 * alpha_param];

    int base_R_5 = (base_color_rgb565 & 0xF800)>>11, base_G_6=(base_color_rgb565 & 0x07E0)>>5, base_B_5=(base_color_rgb565 & 0x001F);
    int base_G_5_tbl = base_G_6 >> 1;
    int fog_R_5  = (fog_color_rgb565 & 0xF800)>>11, fog_G_6 =(fog_color_rgb565 & 0x07E0)>>5,  fog_B_5 =(fog_color_rgb565 & 0x001F);
    int fog_G_5_tbl = fog_G_6 >> 1;
    
    ushort *current_fb_scanline_ptr = &Engine.frameBuffer[GFX_LINESIZE * poly_scan_top_y];

    for (int y_scan = poly_scan_top_y; y_scan < poly_scan_bottom_y; ++y_scan) {
        int start_x = faceLineStart[y_scan], end_x = faceLineEnd[y_scan];
        if (start_x < 0) start_x = 0; 
        // Asumiendo que end_x de ProcessScanEdge es EXCLUSIVO (un píxel después del último)
        if (end_x > GFX_LINESIZE) end_x = GFX_LINESIZE; 

        if (start_x < end_x) {
            ushort *pixel_write_ptr = &current_fb_scanline_ptr[start_x];
            for (int x_pixel = start_x; x_pixel < end_x; ++x_pixel) {
                int blended_R_5bit = fog_blend_ptr[fog_R_5] + base_color_blend_ptr[base_R_5];
                int blended_G_5bit = fog_blend_ptr[fog_G_5_tbl] + base_color_blend_ptr[base_G_5_tbl];
                int blended_B_5bit = fog_blend_ptr[fog_B_5] + base_color_blend_ptr[base_B_5];
                
                if(blended_R_5bit>31)blended_R_5bit=31; if(blended_G_5bit>31)blended_G_5bit=31; if(blended_B_5bit>31)blended_B_5bit=31;
                int final_G_6bit=(blended_G_5bit<<1); if(final_G_6bit>63)final_G_6bit=63;

                *pixel_write_ptr++ = (ushort)((blended_R_5bit << 11) | (final_G_6bit << 5) | blended_B_5bit);
            }
        }
        current_fb_scanline_ptr += GFX_LINESIZE;
    }
#endif 
}

void DrawTexturedFace(void *v, byte sheetID)
{
    Vertex *verts = (Vertex *)v;
    if (!verts) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;

    // --- Comprobaciones de Culling Básicas --- (igual que DrawFace)
    if (verts[0].x < 0 && verts[1].x < 0 && verts[2].x < 0 && verts[3].x < 0) return;
    if (verts[0].x >= GFX_LINESIZE && verts[1].x >= GFX_LINESIZE && verts[2].x >= GFX_LINESIZE && verts[3].x >= GFX_LINESIZE) return;
    if (verts[0].y < 0 && verts[1].y < 0 && verts[2].y < 0 && verts[3].y < 0) return;
    if (verts[0].y >= SCREEN_YSIZE && verts[1].y >= SCREEN_YSIZE && verts[2].y >= SCREEN_YSIZE && verts[3].y >= SCREEN_YSIZE) return;
    if (verts[0].x == verts[1].x && verts[1].x == verts[2].x && verts[2].x == verts[3].x) return;
    if (verts[0].y == verts[1].y && verts[1].y == verts[2].y && verts[2].y == verts[3].y) return;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    GFXSurface *surface_ptr = &gfxSurface[sheetID];
    byte *texture_data_start_ptr = &graphicData[surface_ptr->dataPosition];
    if (surface_ptr->dataPosition >= GFXDATA_SIZE || 
        (surface_ptr->dataPosition + surface_ptr->width * surface_ptr->height > GFXDATA_SIZE) ) return;

    // --- Ordenar Vértices y Calcular Scanlines (igual que DrawFace) ---
    int v_idx[4] = {0,1,2,3}; for(int i=0;i<3;++i)for(int j=0;j<3-i;++j)if(verts[v_idx[j]].y > verts[v_idx[j+1]].y){int t=v_idx[j];v_idx[j]=v_idx[j+1];v_idx[j+1]=t;}
    Vertex *vA=&verts[v_idx[0]], *vB=&verts[v_idx[1]], *vC=&verts[v_idx[2]], *vD=&verts[v_idx[3]];
    int poly_scan_top_y = vA->y, poly_scan_bottom_y = vD->y;
    if(poly_scan_top_y<0)poly_scan_top_y=0; if(poly_scan_bottom_y>SCREEN_YSIZE)poly_scan_bottom_y=SCREEN_YSIZE;
    if(poly_scan_top_y>=poly_scan_bottom_y)return;
    for(int i=poly_scan_top_y;i<poly_scan_bottom_y;++i){faceLineStart[i]=GFX_LINESIZE+1;faceLineEnd[i]=-1;}
    ProcessScanEdgeUV(vA,vB); ProcessScanEdgeUV(vA,vC); ProcessScanEdgeUV(vA,vD);
    ProcessScanEdgeUV(vB,vC); ProcessScanEdgeUV(vB,vD); ProcessScanEdgeUV(vC,vD);
    // --- Fin Ordenar Vértices ---

    ushort *current_fb_scanline_ptr = &Engine.frameBuffer[GFX_LINESIZE * poly_scan_top_y];
    byte *line_palette_idx_ptr = &gfxLineBuffer[poly_scan_top_y]; 
    int texture_width_shift = surface_ptr->widthShift; 
    int texture_width  = surface_ptr->width;
    int texture_height = surface_ptr->height;

    for (int y_scan = poly_scan_top_y; y_scan < poly_scan_bottom_y; ++y_scan) {
        if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
        line_palette_idx_ptr++;

        int screen_start_x = faceLineStart[y_scan], screen_end_x = faceLineEnd[y_scan];
        long long current_U_fixed = faceLineStartU[y_scan], current_V_fixed = faceLineStartV[y_scan];
        long long end_U_fixed = faceLineEndU[y_scan]; long long end_V_fixed = faceLineEndV[y_scan];

        if (screen_start_x < 0) { /* ... Ajustar current_U/V_fixed por clipping izquierdo ... */ screen_start_x = 0;}
        if (screen_end_x > GFX_LINESIZE) screen_end_x = GFX_LINESIZE;
        
        if (screen_start_x < screen_end_x) {
            int num_pixels = screen_end_x - screen_start_x;
            long long delta_U=0, delta_V=0;
            if (num_pixels > 0) { delta_U=(end_U_fixed-current_U_fixed)/num_pixels; delta_V=(end_V_fixed-current_V_fixed)/num_pixels; }
            ushort *pixel_write_ptr = &current_fb_scanline_ptr[screen_start_x];

            for (int x_count = 0; x_count < num_pixels; ++x_count) {
                int tex_u = (int)(current_U_fixed >> 16), tex_v = (int)(current_V_fixed >> 16);
                if (tex_u >= 0 && tex_u < texture_width && tex_v >= 0 && tex_v < texture_height) {
                    int tex_idx = (tex_v * texture_width) + tex_u; // O (tex_v << texture_width_shift) + tex_u
                    byte color_idx = texture_data_start_ptr[tex_idx];
                    if (color_idx > 0) *pixel_write_ptr = activePalette[color_idx];
                }
                pixel_write_ptr++; current_U_fixed += delta_U; current_V_fixed += delta_V;
            }
        }
        current_fb_scanline_ptr += GFX_LINESIZE;
    }
#endif 
}

void DrawTexturedFaceBlended(void *v, byte sheetID)
{
    Vertex *verts = (Vertex *)v;
    if (!verts) return;
    if (sheetID < 0 || sheetID >= SURFACE_COUNT) return;

    // --- Comprobaciones de Culling Básicas --- (igual que DrawFace)
    if (verts[0].x < 0 && verts[1].x < 0 && verts[2].x < 0 && verts[3].x < 0) return;
    if (verts[0].x >= GFX_LINESIZE && verts[1].x >= GFX_LINESIZE && verts[2].x >= GFX_LINESIZE && verts[3].x >= GFX_LINESIZE) return;
    if (verts[0].y < 0 && verts[1].y < 0 && verts[2].y < 0 && verts[3].y < 0) return;
    if (verts[0].y >= SCREEN_YSIZE && verts[1].y >= SCREEN_YSIZE && verts[2].y >= SCREEN_YSIZE && verts[3].y >= SCREEN_YSIZE) return;
    if (verts[0].x == verts[1].x && verts[1].x == verts[2].x && verts[2].x == verts[3].x) return;
    if (verts[0].y == verts[1].y && verts[1].y == verts[2].y && verts[2].y == verts[3].y) return;

#if RETRO_SOFTWARE_RENDER
    if (!Engine.frameBuffer) return;
    GFXSurface *surface_ptr = &gfxSurface[sheetID];
    byte *texture_data_start_ptr = &graphicData[surface_ptr->dataPosition];
    if (surface_ptr->dataPosition >= GFXDATA_SIZE || 
        (surface_ptr->dataPosition + surface_ptr->width * surface_ptr->height > GFXDATA_SIZE) ) return;

    // --- Ordenar Vértices y Calcular Scanlines (igual que DrawTexturedFace) ---
    int v_idx[4] = {0,1,2,3}; for(int i=0;i<3;++i)for(int j=0;j<3-i;++j)if(verts[v_idx[j]].y > verts[v_idx[j+1]].y){int t=v_idx[j];v_idx[j]=v_idx[j+1];v_idx[j+1]=t;}
    Vertex *vA=&verts[v_idx[0]], *vB=&verts[v_idx[1]], *vC=&verts[v_idx[2]], *vD=&verts[v_idx[3]];
    int poly_scan_top_y = vA->y, poly_scan_bottom_y = vD->y;
    if(poly_scan_top_y<0)poly_scan_top_y=0; if(poly_scan_bottom_y>SCREEN_YSIZE)poly_scan_bottom_y=SCREEN_YSIZE;
    if(poly_scan_top_y>=poly_scan_bottom_y)return;
    for(int i=poly_scan_top_y;i<poly_scan_bottom_y;++i){faceLineStart[i]=GFX_LINESIZE+1;faceLineEnd[i]=-1;}
    ProcessScanEdgeUV(vA,vB); ProcessScanEdgeUV(vA,vC); ProcessScanEdgeUV(vA,vD);
    ProcessScanEdgeUV(vB,vC); ProcessScanEdgeUV(vB,vD); ProcessScanEdgeUV(vC,vD);
    // --- Fin Ordenar Vértices ---

    ushort *current_fb_scanline_ptr = &Engine.frameBuffer[GFX_LINESIZE * poly_scan_top_y];
    byte *line_palette_idx_ptr = &gfxLineBuffer[poly_scan_top_y]; 
    int texture_width = surface_ptr->width, texture_height = surface_ptr->height;

    for (int y_scan = poly_scan_top_y; y_scan < poly_scan_bottom_y; ++y_scan) {
        if (line_palette_idx_ptr < &gfxLineBuffer[GFXLINEBUFFER_SIZE]) activePalette = fullPalette[*line_palette_idx_ptr];
        line_palette_idx_ptr++;

        int screen_start_x = faceLineStart[y_scan], screen_end_x = faceLineEnd[y_scan];
        long long current_U_fixed = faceLineStartU[y_scan], current_V_fixed = faceLineStartV[y_scan];
        long long end_U_fixed = faceLineEndU[y_scan]; long long end_V_fixed = faceLineEndV[y_scan];

        if (screen_start_x < 0) { /* ... Ajustar current_U/V_fixed ... */ screen_start_x = 0;}
        if (screen_end_x > GFX_LINESIZE) screen_end_x = GFX_LINESIZE;
        
        if (screen_start_x < screen_end_x) {
            int num_pixels = screen_end_x - screen_start_x;
            long long delta_U=0, delta_V=0;
            if (num_pixels > 0) { delta_U=(end_U_fixed-current_U_fixed)/num_pixels; delta_V=(end_V_fixed-current_V_fixed)/num_pixels; }
            ushort *pixel_write_ptr = &current_fb_scanline_ptr[screen_start_x];

            for (int x_count = 0; x_count < num_pixels; ++x_count) {
                int tex_u = (int)(current_U_fixed >> 16), tex_v = (int)(current_V_fixed >> 16);
                if (tex_u >= 0 && tex_u < texture_width && tex_v >= 0 && tex_v < texture_height) {
                    int tex_idx = (tex_v * texture_width) + tex_u;
                    byte sprite_color_idx = texture_data_start_ptr[tex_idx];
                    if (sprite_color_idx > 0) {
                        ushort sprite_color_rgb565 = activePalette[sprite_color_idx];
                        ushort framebuffer_color_rgb565 = *pixel_write_ptr;
                        int sr=(sprite_color_rgb565&0xF800)>>11, sg=(sprite_color_rgb565&0x07E0)>>5, sb=(sprite_color_rgb565&0x001F);
                        int dr=(framebuffer_color_rgb565&0xF800)>>11, dg=(framebuffer_color_rgb565&0x07E0)>>5, db=(framebuffer_color_rgb565&0x001F);
                        int rA=(sr+dr)>>1, gA=(sg+dg)>>1, bA=(sb+db)>>1;
                        *pixel_write_ptr = (ushort)((rA<<11)|(gA<<5)|bA);
                    }
                }
                pixel_write_ptr++; current_U_fixed += delta_U; current_V_fixed += delta_V;
            }
        }
        current_fb_scanline_ptr += GFX_LINESIZE;
    }
#endif 
}

#if RETRO_REV00 || RETRO_REV01 
void DrawBitmapText(void *menuData, 
                    int XPos_screen, int YPos_screen, 
                    int scale_factor, 
                    int line_spacing, 
                    int start_row_idx, 
                    int num_rows_to_draw)
{
    if (!menuData) return;
    TextMenu *text_menu_ptr = (TextMenu *)menuData;

    if (num_rows_to_draw < 0 || start_row_idx < 0 || start_row_idx >= text_menu_ptr->rowCount) {
        num_rows_to_draw = text_menu_ptr->rowCount - start_row_idx;
    }
    if (start_row_idx + num_rows_to_draw > text_menu_ptr->rowCount) {
        num_rows_to_draw = text_menu_ptr->rowCount - start_row_idx;
    }
    if (num_rows_to_draw <= 0) return;

    long long current_Y_fixed = (long long)YPos_screen << 9;

    for (int row_iter = 0; row_iter < num_rows_to_draw; ++row_iter) {
        int current_row_index = start_row_idx + row_iter;
        if (current_row_index >= text_menu_ptr->rowCount || current_row_index >= MAX_MENU_ROWS) break;

        long long current_X_fixed = (long long)XPos_screen << 9;
        int entry_offset = text_menu_ptr->entryStart[current_row_index];
        int entry_char_count = text_menu_ptr->entrySize[current_row_index];

        for (int char_idx = 0; char_idx < entry_char_count; ++char_idx) {
            if (entry_offset + char_idx >= MAX_TEXTMENU_CHARS) break; 
            ushort char_code = text_menu_ptr->textData[entry_offset + char_idx];
            if (char_code >= MAX_FONT_CHARS) continue; 

            FontCharacter *font_char_ptr = &fontCharacterList[char_code];
            if (textMenuSurfaceNo < 0 || textMenuSurfaceNo >= SURFACE_COUNT) break;

#if RETRO_SOFTWARE_RENDER
            DrawSpriteScaled(FLIP_NONE, 
                             (int)(current_X_fixed >> 9), (int)(current_Y_fixed >> 9), 
                             -font_char_ptr->pivotX, -font_char_ptr->pivotY, 
                             scale_factor, scale_factor, 
                             font_char_ptr->width, font_char_ptr->height, 
                             font_char_ptr->srcX, font_char_ptr->srcY, 
                             textMenuSurfaceNo); 
#endif
            // Asumiendo scale_factor es 0x200 para 1.0x, y xAdvance es píxeles enteros.
            // X es 23.9 fijo. Avance = (entero * 0x200) -> resultado .9 fijo.
            current_X_fixed += (long long)font_char_ptr->xAdvance * scale_factor;
        }
        current_Y_fixed += (long long)line_spacing * scale_factor;
    }
}
#endif

void DrawTextMenuEntry(void *menuData, 
                       int row_index,  
                       int XPos_screen, int YPos_screen, 
                       int y_offset_for_highlight) 
{
    if (!menuData) return;
    TextMenu *text_menu_ptr = (TextMenu *)menuData; 

    if (row_index < 0 || row_index >= text_menu_ptr->rowCount || row_index >= MAX_MENU_ROWS) return;
    
    int char_data_start_offset = text_menu_ptr->entryStart[row_index]; 
    int num_chars_in_entry = text_menu_ptr->entrySize[row_index];
    int x_centering_offset = 0;

    // Asumiendo que alignment 2 es ALIGN_CENTER
    if (text_menu_ptr->alignment == 2 /* ALIGN_CENTER */ && (num_chars_in_entry % 2 != 0)) {
        x_centering_offset = -4; 
    }

    for (int i = 0; i < num_chars_in_entry; ++i) {
        int current_char_data_offset = char_data_start_offset + i;
        if (current_char_data_offset >= MAX_TEXTMENU_CHARS) break; 

        ushort char_data = text_menu_ptr->textData[current_char_data_offset];
        int char_spr_x = (char_data & 0x0F) << 3;       
        int char_spr_y = ((char_data >> 4) & 0x0F) << 3; // Asegurar que solo se usen 4 bits para la fila también (0-15)
                                                       // Si la hoja de fuente puede tener más de 16 filas de caracteres.
        char_spr_y += y_offset_for_highlight;
        int final_XPos_screen = XPos_screen + (i << 3) + x_centering_offset;

        if (textMenuSurfaceNo < 0 || textMenuSurfaceNo >= SURFACE_COUNT) break; 

        DrawSprite(final_XPos_screen, YPos_screen, 8, 8, 
                   char_spr_x, char_spr_y, textMenuSurfaceNo);
    }
}

void DrawStageTextEntry(void *menuData, 
                        int row_index,  
                        int XPos_screen, int YPos_screen, 
                        int y_offset_for_highlight) 
{
    if (!menuData) return;
    TextMenu *text_menu_ptr = (TextMenu *)menuData; 

    if (row_index < 0 || row_index >= text_menu_ptr->rowCount || row_index >= MAX_MENU_ROWS) return;
    
    int char_data_start_offset = text_menu_ptr->entryStart[row_index]; 
    int num_chars_in_entry = text_menu_ptr->entrySize[row_index];

    for (int i = 0; i < num_chars_in_entry; ++i) {
        int current_char_data_offset = char_data_start_offset + i;
        if (current_char_data_offset >= MAX_TEXTMENU_CHARS) break; 

        ushort char_data = text_menu_ptr->textData[current_char_data_offset];
        int char_spr_x = (char_data & 0x0F) << 3;       
        int char_spr_y_base = ((char_data >> 4) & 0x0F) << 3; 
        
        int final_char_spr_y;
        if (i == num_chars_in_entry - 1) { // Último carácter
            final_char_spr_y = char_spr_y_base;
        } else { // No es el último carácter
            final_char_spr_y = char_spr_y_base + y_offset_for_highlight;
        }
        
        int final_XPos_screen = XPos_screen + (i << 3);

        if (textMenuSurfaceNo < 0 || textMenuSurfaceNo >= SURFACE_COUNT) break; 

        DrawSprite(final_XPos_screen, YPos_screen, 8, 8, 
                   char_spr_x, final_char_spr_y, textMenuSurfaceNo);
    }
}

void DrawBlendedTextMenuEntry(void *menuData, 
                              int row_index,  
                              int XPos_screen, int YPos_screen, 
                              int y_offset_for_highlight) 
{
    if (!menuData) return;
    TextMenu *text_menu_ptr = (TextMenu *)menuData; 

    if (row_index < 0 || row_index >= text_menu_ptr->rowCount || row_index >= MAX_MENU_ROWS) return;
    
    int char_data_start_offset = text_menu_ptr->entryStart[row_index]; 
    int num_chars_in_entry = text_menu_ptr->entrySize[row_index];
    int x_centering_offset = 0;

    if (text_menu_ptr->alignment == 2 /* ALIGN_CENTER */ && (num_chars_in_entry % 2 != 0)) {
        x_centering_offset = -4; 
    }

    for (int i = 0; i < num_chars_in_entry; ++i) {
        int current_char_data_offset = char_data_start_offset + i;
        if (current_char_data_offset >= MAX_TEXTMENU_CHARS) break; 

        ushort char_data = text_menu_ptr->textData[current_char_data_offset];
        int char_spr_x = (char_data & 0x0F) << 3;       
        int char_spr_y = ((char_data >> 4) & 0x0F) << 3; 
        char_spr_y += y_offset_for_highlight;
        int final_XPos_screen = XPos_screen + (i << 3) + x_centering_offset;

        if (textMenuSurfaceNo < 0 || textMenuSurfaceNo >= SURFACE_COUNT) break; 

        DrawBlendedSprite(final_XPos_screen, YPos_screen, 8, 8, 
                          char_spr_x, char_spr_y, textMenuSurfaceNo);
    }
}

void DrawTextMenu(void *menuData, 
                  int XPos_base, int YPos_base) 
{
    if (!menuData) return;
    TextMenu *text_menu_ptr = (TextMenu *)menuData; 

    int rows_to_iterate;
    int start_row_offset = text_menu_ptr->visibleRowOffset;

    if (text_menu_ptr->visibleRowCount > 0) {
        if (start_row_offset < 0) start_row_offset = 0;
        if (start_row_offset >= text_menu_ptr->rowCount) start_row_offset = text_menu_ptr->rowCount > 0 ? text_menu_ptr->rowCount -1 : 0;
        rows_to_iterate = text_menu_ptr->visibleRowCount;
        if (start_row_offset + rows_to_iterate > text_menu_ptr->rowCount) {
            rows_to_iterate = text_menu_ptr->rowCount - start_row_offset;
        }
    } else { 
        start_row_offset = 0;
        text_menu_ptr->visibleRowOffset = 0; 
        rows_to_iterate = text_menu_ptr->rowCount;
    }
    if (rows_to_iterate <= 0) return;

    if (text_menu_ptr->selectionCount == 3) {
        text_menu_ptr->selection2 = -1; 
        for (int i = 0; i <= text_menu_ptr->selection1 && i < text_menu_ptr->rowCount && i < MAX_MENU_ROWS; ++i) { 
            if (text_menu_ptr->entryHighlight[i]) {
                text_menu_ptr->selection2 = i;
            }
        }
    }
    
    int current_YPos_screen = YPos_base; 

    for (int i_loop = 0; i_loop < rows_to_iterate; ++i_loop) {
        int current_row_index = start_row_offset + i_loop;
        if (current_row_index >= text_menu_ptr->rowCount || current_row_index >= MAX_MENU_ROWS) break; 

        int current_XPos_for_row = XPos_base; 
        // Asumiendo que entrySize es un array y MAX_MENU_ENTRIES_SIZE_ARRAY es su tamaño.
        if (current_row_index < MAX_MENU_ENTRIES_SIZE_ARRAY) { 
            switch (text_menu_ptr->alignment) {
                case 1: /*ALIGN_RIGHT*/ current_XPos_for_row = XPos_base - (text_menu_ptr->entrySize[current_row_index] << 3); break;
                case 2: /*ALIGN_CENTER*/current_XPos_for_row = XPos_base - (text_menu_ptr->entrySize[current_row_index] >> 1 << 3); break;
            }
        }

        int highlight_offset_y = 0; 
        switch (text_menu_ptr->selectionCount) {
            case 1: 
                if (current_row_index == text_menu_ptr->selection1) highlight_offset_y = 128;
                DrawTextMenuEntry(text_menu_ptr, current_row_index, current_XPos_for_row, current_YPos_screen, highlight_offset_y);
                break;
            case 2: 
                if (current_row_index == text_menu_ptr->selection1 || current_row_index == text_menu_ptr->selection2) highlight_offset_y = 128;
                DrawTextMenuEntry(text_menu_ptr, current_row_index, current_XPos_for_row, current_YPos_screen, highlight_offset_y);
                break;
            case 3: 
                if (current_row_index == text_menu_ptr->selection1) {
                    DrawTextMenuEntry(text_menu_ptr, current_row_index, current_XPos_for_row, current_YPos_screen, 128);
                } else if (current_row_index == text_menu_ptr->selection2) { 
                    DrawStageTextEntry(text_menu_ptr, current_row_index, current_XPos_for_row, current_YPos_screen, 128);
                } else { 
                    DrawTextMenuEntry(text_menu_ptr, current_row_index, current_XPos_for_row, current_YPos_screen, 0);
                }
                break;
            default: 
                 DrawTextMenuEntry(text_menu_ptr, current_row_index, current_XPos_for_row, current_YPos_screen, 0);
                 break;
        }
        current_YPos_screen += 8; 
    }
}

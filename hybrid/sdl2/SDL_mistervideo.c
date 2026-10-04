/*
  SDL video, keyboard, mouse and joystick driver for MiSTer hybrid cores.

  The game's window is the 320x200 picture the FPGA core scans out at 15kHz
  (hybrid/rtl/hybrid_host.sv); input comes from the core through the same
  shared memory. See mister_hybrid.h.

  Hints (also read from the environment):
    SDL_MISTER_VIDEO_FORMAT  "RGB565" (default) or "INDEX8". With INDEX8 the
                             window surface is paletted: set its colours with
                             SDL_SetPaletteColors(surface->format->palette).
                             Only for games that draw to the window surface;
                             the 2D render API needs RGB565.
    SDL_MISTER_VSYNC         "1": SDL_UpdateWindowSurface() shows at most one
                             frame per video field (59.6Hz)

  Joysticks: two virtual joysticks (players 1 and 2) with 4 axes (left stick,
  right stick), one hat (d-pad) and 30 buttons. Buttons 0..27 are the core's
  buttons in the order of its "J1," list; buttons 28 and 29 of player 1 are
  "Menu OK" and "Menu Back", see MH_MenuButtons().

  This software is provided 'as-is' under the zlib license, like SDL itself.
*/
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_MISTER

#include "SDL.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_hints.h"
#include "SDL_joystick.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"

#include "mister_hybrid.h"

#define MISTER_SURFACE "_SDL_MiSTerSurface"

#define MISTER_NUM_JOYSTICKS  2
#define MISTER_NUM_BUTTONS    30
#define MISTER_BUTTON_MENU_OK   28
#define MISTER_BUTTON_MENU_BACK 29

static Uint32 video_format;
static SDL_bool vsync;
static Uint32 palette_version;
static SDL_bool palette_valid;
static Uint32 last_field;
/* frame for windows that are not 320x200: cropped or centred */
static Uint8 *fit_buffer;

static SDL_bool input_valid;
static mh_input last_input;
static SDL_bool quit_sent;

static int joystick_index[MISTER_NUM_JOYSTICKS] = { -1, -1 };
static SDL_Joystick *joysticks[MISTER_NUM_JOYSTICKS];

/* Joysticks */

static void SDLCALL MISTER_JoystickUpdate(void *userdata)
{
    const int player = (int)(intptr_t)userdata;
    SDL_Joystick *joystick = joysticks[player];
    mh_input input;
    Uint32 joy, ok_bits = 0, back_bits = 0;
    Uint8 hat = SDL_HAT_CENTERED;
    int i;

    if (!joystick) {
        return;
    }
    MH_ReadInput(&input);
    joy = input.joystick[player];

    SDL_JoystickSetVirtualAxis(joystick, 0, (Sint16)(input.analog_lx[player] * 256));
    SDL_JoystickSetVirtualAxis(joystick, 1, (Sint16)(input.analog_ly[player] * 256));
    SDL_JoystickSetVirtualAxis(joystick, 2, (Sint16)(input.analog_rx[player] * 256));
    SDL_JoystickSetVirtualAxis(joystick, 3, (Sint16)(input.analog_ry[player] * 256));

    if (joy & MH_JOY_UP) {
        hat |= SDL_HAT_UP;
    }
    if (joy & MH_JOY_DOWN) {
        hat |= SDL_HAT_DOWN;
    }
    if (joy & MH_JOY_LEFT) {
        hat |= SDL_HAT_LEFT;
    }
    if (joy & MH_JOY_RIGHT) {
        hat |= SDL_HAT_RIGHT;
    }
    SDL_JoystickSetVirtualHat(joystick, 0, hat);

    for (i = 0; i < MISTER_BUTTON_MENU_OK; i++) {
        SDL_JoystickSetVirtualButton(joystick, i, (joy & MH_JOY_BUTTON(i)) ? SDL_PRESSED : SDL_RELEASED);
    }
    if (player == 0) {
        MH_MenuButtons(input.osd_status, &ok_bits, &back_bits);
    }
    SDL_JoystickSetVirtualButton(joystick, MISTER_BUTTON_MENU_OK, (joy & ok_bits) ? SDL_PRESSED : SDL_RELEASED);
    SDL_JoystickSetVirtualButton(joystick, MISTER_BUTTON_MENU_BACK, (joy & back_bits) ? SDL_PRESSED : SDL_RELEASED);
}

static void MISTER_InitJoysticks(void)
{
    static const char *const names[MISTER_NUM_JOYSTICKS] = { "MiSTer Joystick 1", "MiSTer Joystick 2" };
    SDL_VirtualJoystickDesc desc;
    int i;

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
        return;
    }
    for (i = 0; i < MISTER_NUM_JOYSTICKS; i++) {
        SDL_zero(desc);
        desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        desc.type = SDL_JOYSTICK_TYPE_UNKNOWN;
        desc.naxes = 4;
        desc.nbuttons = MISTER_NUM_BUTTONS;
        desc.nhats = 1;
        desc.name = names[i];
        desc.userdata = (void *)(intptr_t)i;
        desc.Update = MISTER_JoystickUpdate;
        joystick_index[i] = SDL_JoystickAttachVirtualEx(&desc);
        if (joystick_index[i] >= 0) {
            /* kept open: the virtual joystick is fed through this handle */
            joysticks[i] = SDL_JoystickOpen(joystick_index[i]);
        }
    }
}

static void MISTER_QuitJoysticks(void)
{
    int i;

    for (i = MISTER_NUM_JOYSTICKS - 1; i >= 0; i--) {
        if (joysticks[i]) {
            SDL_JoystickClose(joysticks[i]);
            joysticks[i] = NULL;
        }
        if (joystick_index[i] >= 0) {
            SDL_JoystickDetachVirtual(joystick_index[i]);
            joystick_index[i] = -1;
        }
    }
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

/* Keyboard and mouse */

/* Text for a key press on a US keyboard, 0 if it types nothing */
static char MISTER_KeyText(SDL_Scancode scancode)
{
    static const char digits_shifted[] = ")!@#$%^&*(";
    const SDL_Keymod mod = SDL_GetModState();
    const SDL_bool shift = (mod & KMOD_SHIFT) != 0;

    if (mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) {
        return 0;
    }
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        const SDL_bool upper = shift != ((mod & KMOD_CAPS) != 0);
        return (char)((upper ? 'A' : 'a') + (scancode - SDL_SCANCODE_A));
    }
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_0) {
        const int digit = (scancode - SDL_SCANCODE_1 + 1) % 10;
        return shift ? digits_shifted[digit] : (char)('0' + digit);
    }
    switch (scancode) {
    case SDL_SCANCODE_SPACE:        return ' ';
    case SDL_SCANCODE_MINUS:        return shift ? '_' : '-';
    case SDL_SCANCODE_EQUALS:       return shift ? '+' : '=';
    case SDL_SCANCODE_LEFTBRACKET:  return shift ? '{' : '[';
    case SDL_SCANCODE_RIGHTBRACKET: return shift ? '}' : ']';
    case SDL_SCANCODE_BACKSLASH:    return shift ? '|' : '\\';
    case SDL_SCANCODE_SEMICOLON:    return shift ? ':' : ';';
    case SDL_SCANCODE_APOSTROPHE:   return shift ? '"' : '\'';
    case SDL_SCANCODE_GRAVE:        return shift ? '~' : '`';
    case SDL_SCANCODE_COMMA:        return shift ? '<' : ',';
    case SDL_SCANCODE_PERIOD:       return shift ? '>' : '.';
    case SDL_SCANCODE_SLASH:        return shift ? '?' : '/';
    case SDL_SCANCODE_KP_DIVIDE:    return '/';
    case SDL_SCANCODE_KP_MULTIPLY:  return '*';
    case SDL_SCANCODE_KP_MINUS:     return '-';
    case SDL_SCANCODE_KP_PLUS:      return '+';
    default:
        break;
    }
    if (scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_0 && (mod & KMOD_NUM)) {
        return (char)('0' + (scancode - SDL_SCANCODE_KP_1 + 1) % 10);
    }
    return 0;
}

static int MISTER_SetRelativeMouseMode(SDL_bool enabled)
{
    /* the core only reports movement */
    (void)enabled;
    return 0;
}

static void MISTER_PumpEvents(_THIS)
{
    SDL_Window *window = SDL_GetKeyboardFocus();
    mh_input input;
    int i;

    (void)_this;
    if (!MH_CheckAlive()) {
        /* another core was loaded: the game has to go */
        if (!quit_sent) {
            quit_sent = SDL_TRUE;
            SDL_SendQuit();
        }
        return;
    }
    MH_ReadInput(&input);
    if (!input_valid) {
        /* keys held and mouse movement from before the game started don't count */
        input_valid = SDL_TRUE;
        last_input = input;
        SDL_zeroa(last_input.keys);
        last_input.mouse_buttons = 0;
    }

    for (i = 0; i < 512; i++) {
        const Uint32 bit = 1u << (i & 0x1f);
        const Uint32 changed = (input.keys[i >> 5] ^ last_input.keys[i >> 5]);

        if (changed == 0) {
            i |= 0x1f;
            continue;
        }
        if (changed & bit) {
            const SDL_Scancode scancode = (SDL_Scancode)MH_KeyToHID(i);
            const SDL_bool pressed = (input.keys[i >> 5] & bit) != 0;

            if (scancode != SDL_SCANCODE_UNKNOWN) {
                SDL_SendKeyboardKey(pressed ? SDL_PRESSED : SDL_RELEASED, scancode);
                if (pressed) {
                    char text[2] = { MISTER_KeyText(scancode), 0 };
                    if (text[0]) {
                        SDL_SendKeyboardText(text);
                    }
                }
            }
        }
    }

    if (input.mouse_x != last_input.mouse_x || input.mouse_y != last_input.mouse_y) {
        /* PS/2 counts y upwards */
        SDL_SendMouseMotion(window, 0, 1, input.mouse_x - last_input.mouse_x, last_input.mouse_y - input.mouse_y);
    }
    if (input.mouse_wheel != last_input.mouse_wheel) {
        SDL_SendMouseWheel(window, 0, 0.0f, (float)(Sint16)(input.mouse_wheel - last_input.mouse_wheel), SDL_MOUSEWHEEL_NORMAL);
    }
    if (input.mouse_buttons != last_input.mouse_buttons) {
        static const Uint8 buttons[3] = { SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT, SDL_BUTTON_MIDDLE };
        const int changed = input.mouse_buttons ^ last_input.mouse_buttons;

        for (i = 0; i < 3; i++) {
            if (changed & (1 << i)) {
                SDL_SendMouseButton(window, 0, (input.mouse_buttons & (1 << i)) ? SDL_PRESSED : SDL_RELEASED, buttons[i]);
            }
        }
    }
    last_input = input;
}

/* Window framebuffer */

static void MISTER_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface;

    (void)_this;
    surface = (SDL_Surface *)SDL_SetWindowData(window, MISTER_SURFACE, NULL);
    SDL_FreeSurface(surface);
}

static int MISTER_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_Surface *surface;
    int w, h;

    MISTER_DestroyWindowFramebuffer(_this, window);

    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, video_format);
    if (!surface) {
        return -1;
    }
    SDL_SetWindowData(window, MISTER_SURFACE, surface);
    *format = video_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;

    MH_SetFormat(video_format == SDL_PIXELFORMAT_INDEX8 ? MH_FORMAT_INDEX8 : MH_FORMAT_RGB565);
    palette_valid = SDL_FALSE;
    return 0;
}

static int MISTER_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_Surface *surface = (SDL_Surface *)SDL_GetWindowData(window, MISTER_SURFACE);
    const SDL_Palette *palette;
    const Uint8 *pixels;
    int pitch;

    (void)_this;
    (void)rects;
    (void)numrects;
    if (!surface) {
        return SDL_SetError("Couldn't find the MiSTer surface for the window");
    }

    /* the application sets the colours on the surface SDL made from our pixels */
    palette = window->surface ? window->surface->format->palette : NULL;
    if (palette && (!palette_valid || palette->version != palette_version)) {
        Uint32 rgb[256];
        int i;

        for (i = 0; i < 256; i++) {
            if (i < palette->ncolors) {
                rgb[i] = (palette->colors[i].r << 16) | (palette->colors[i].g << 8) | palette->colors[i].b;
            } else {
                rgb[i] = 0;
            }
        }
        MH_SetPalette(rgb);
        palette_version = palette->version;
        palette_valid = SDL_TRUE;
    }

    pixels = surface->pixels;
    pitch = surface->pitch;
    if (surface->w != MH_WIDTH || surface->h != MH_HEIGHT) {
        /* not the size of the screen: show the middle of it, or centre it */
        const int bpp = surface->format->BytesPerPixel;
        const int w = SDL_min(surface->w, MH_WIDTH);
        const int h = SDL_min(surface->h, MH_HEIGHT);
        const Uint8 *src = pixels + (surface->h - h) / 2 * pitch + (surface->w - w) / 2 * bpp;
        Uint8 *dst;
        int y;

        if (!fit_buffer) {
            fit_buffer = (Uint8 *)SDL_calloc(MH_WIDTH * MH_HEIGHT, 2);
            if (!fit_buffer) {
                return SDL_OutOfMemory();
            }
        }
        dst = fit_buffer + (MH_HEIGHT - h) / 2 * MH_WIDTH * bpp + (MH_WIDTH - w) / 2 * bpp;
        for (y = 0; y < h; y++) {
            SDL_memcpy(dst + y * MH_WIDTH * bpp, src + y * pitch, w * bpp);
        }
        pixels = fit_buffer;
        pitch = MH_WIDTH * bpp;
    }

    if (vsync) {
        MH_WaitField(last_field);
        last_field = MH_FieldCounter();
    }
    MH_Present(pixels, pitch);
    return 0;
}

/* Windows */

static int MISTER_CreateWindow(_THIS, SDL_Window *window)
{
    (void)_this;
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);
    return 0;
}

static int MISTER_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    (void)_this;
    (void)display;
    (void)mode;
    return 0;
}

/* Device */

static int MISTER_VideoInit(_THIS)
{
    const char *hint;
    SDL_DisplayMode mode;

    video_format = SDL_PIXELFORMAT_RGB565;
    hint = SDL_GetHint("SDL_MISTER_VIDEO_FORMAT");
    if (hint && SDL_strcasecmp(hint, "INDEX8") == 0) {
        video_format = SDL_PIXELFORMAT_INDEX8;
    }
    vsync = SDL_GetHintBoolean("SDL_MISTER_VSYNC", SDL_FALSE);
    input_valid = SDL_FALSE;
    quit_sent = SDL_FALSE;

    SDL_zero(mode);
    mode.format = video_format;
    mode.w = MH_WIDTH;
    mode.h = MH_HEIGHT;
    mode.refresh_rate = 60;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }
    SDL_AddDisplayMode(&_this->displays[0], &mode);

    SDL_GetMouse()->SetRelativeMouseMode = MISTER_SetRelativeMouseMode;
    MISTER_InitJoysticks();
    return 0;
}

static void MISTER_VideoQuit(_THIS)
{
    (void)_this;
    MISTER_QuitJoysticks();
    SDL_free(fit_buffer);
    fit_buffer = NULL;
    MH_Close();
}

static void MISTER_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *MISTER_CreateDevice(void)
{
    SDL_VideoDevice *device;

    /* only with the core loaded: SDL tries the next driver otherwise */
    if (!MH_Open()) {
        return NULL;
    }

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }
    /* no GPU: window surfaces are never backed by a texture */
    device->is_dummy = SDL_TRUE;

    device->VideoInit = MISTER_VideoInit;
    device->VideoQuit = MISTER_VideoQuit;
    device->SetDisplayMode = MISTER_SetDisplayMode;
    device->PumpEvents = MISTER_PumpEvents;
    device->CreateSDLWindow = MISTER_CreateWindow;
    device->CreateWindowFramebuffer = MISTER_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = MISTER_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = MISTER_DestroyWindowFramebuffer;
    device->free = MISTER_DeleteDevice;

    return device;
}

VideoBootStrap MISTER_bootstrap = {
    "mister", "MiSTer hybrid core video driver",
    MISTER_CreateDevice,
    NULL /* no ShowMessageBox implementation */
};

#endif /* SDL_VIDEO_DRIVER_MISTER */

/* vi: set ts=4 sw=4 expandtab: */

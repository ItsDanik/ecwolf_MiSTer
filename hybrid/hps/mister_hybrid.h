#ifndef MISTER_HYBRID_H
#define MISTER_HYBRID_H

// HPS side of a MiSTer hybrid core: the game runs on the ARM and talks to the
// FPGA core (hybrid/rtl/hybrid_host.sv, which documents the memory layout)
// through shared DDR3 memory. The core scans out 320x200 frames at 15kHz,
// plays a 44.1kHz audio ring and publishes keyboard, mouse, joystick and OSD
// state.
//
// Environment variables:
//   MISTER_HYBRID_CORE  name of the core as in /tmp/CORENAME (the name in its
//                       CONF_STR). Set by the launcher; MH_Open() only attaches
//                       to this core and MH_CheckAlive() notices when another
//                       one is loaded. It is also the prefix of the core's
//                       joystick mapping files.
//   MISTER_HYBRID_JN    the core's default button mapping, the "jn," list of
//                       its CONF_STR ("A,B,X,Y,L,R,Select,Start" if unset)
//   MISTER_HYBRID_SHM   development: a file to use instead of the DDR3 window
//                       (see hybrid/tools/fakecore.c)

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MH_WIDTH 320
#define MH_HEIGHT 200

#define MH_FORMAT_INDEX8 0 // 8bpp, palette from MH_SetPalette()
#define MH_FORMAT_RGB565 1

#define MH_AUDIO_RATE 44100
#define MH_AUDIO_RING_FRAMES 16384

// MiSTer joystick word: directions, then the buttons of the core's "J1," list
#define MH_JOY_RIGHT (1u << 0)
#define MH_JOY_LEFT (1u << 1)
#define MH_JOY_DOWN (1u << 2)
#define MH_JOY_UP (1u << 3)
#define MH_JOY_BUTTON(n) (1u << (4 + (n)))

// OSD status bits every hybrid core lays out the same way (see
// hybrid/README.md). Bits 24..63 belong to the game. The first entry of an
// option is 0 and so is the default.
#define MH_OSD_SOUND_VOLUME(s) ((int)(((s) >> 7) & 0xF))  // 0 = 100%, 10 = 0%
#define MH_OSD_MUSIC_VOLUME(s) ((int)(((s) >> 11) & 0xF)) // 0 = 100%, 10 = 0%
// 0 = the MiSTer menu's OK/Back buttons, 1.. = A, B, X, Y, L, R, Select, Start
#define MH_OSD_MENU_OK(s) ((int)(((s) >> 16) & 0xF))
#define MH_OSD_MENU_BACK(s) ((int)(((s) >> 20) & 0xF))
#define MH_OSD_GAME_BITS(s, lsb, width) ((int)(((s) >> (lsb)) & ((1u << (width)) - 1)))

typedef struct mh_input {
    uint32_t frame;        // field counter, increments every vblank (59.6Hz)
    uint32_t joystick[2];  // MH_JOY_* bits
    int8_t analog_lx[2];   // left stick, -128..127
    int8_t analog_ly[2];
    int8_t analog_rx[2];   // right stick
    int8_t analog_ry[2];
    uint64_t osd_status;   // OSD option bits [63:0]
    int32_t mouse_x;       // accumulated PS/2 mouse movement (y positive up)
    int32_t mouse_y;
    int16_t mouse_wheel;   // accumulated wheel movement
    int mouse_buttons;     // bit 0 left, bit 1 right, bit 2 middle
    uint32_t keys[16];     // PS/2 set 2 key bitmap, bit index = extended << 8 | code
} mh_input;

// Returns 1 if the core is loaded and running. Without it everything below is
// a no-op, so a game can run headless.
int MH_Open(void);
void MH_Close(void);
int MH_IsOpen(void);
// Returns 0 (and detaches for good) once the core is no longer loaded
int MH_CheckAlive(void);
const char* MH_CoreName(void);

// Video. The picture stays on the core's test pattern until the first frame.
void MH_SetFormat(int format);
// Show a 320x200 frame. `pitch` is the source's row size in bytes
void MH_Present(const void* pixels, int pitch);
// 256 entries of 0x00RRGGBB
void MH_SetPalette(const uint32_t* rgb);
uint32_t MH_FieldCounter(void);
// Sleep until the field counter is past `field`
void MH_WaitField(uint32_t field);

// Input
void MH_ReadInput(mh_input* input);
uint64_t MH_OSDStatus(void);
// Core buttons (MH_JOY_BUTTON bits) driven by the physical buttons the OSD's
// Menu OK / Menu Back options name. MiSTer cannot map one physical button to
// two core buttons, so this is how a button with a game function can also
// confirm and cancel in menus. Cheap to call every frame.
void MH_MenuButtons(uint64_t osd_status, uint32_t* ok_bits, uint32_t* back_bits);
// USB HID usage (= SDL scancode) of a key bitmap index, 0 if there is none
int MH_KeyToHID(int key_index);

// Audio: stereo frames {R << 16 | L}, played by the core at MH_AUDIO_RATE
void MH_AudioEnable(int enabled);
// Blocks until the core has less than `lead` frames left to play, then queues
// `count` frames. The core's sample clock paces the caller.
void MH_AudioWrite(const uint32_t* frames, int count, int lead);

// Screens every hybrid core shares (mister_ui.c), drawn straight to the core;
// they work before the game has set up anything.
// A list to pick from, with the keyboard or joystick 1. Returns the index of
// the chosen item, or -1 for Back/Esc or when the core is gone.
int MH_UI_Menu(const char* title, const char* prompt, const char* const* items, int count, int selected);
#define MH_UI_WAIT 1  // until a key or button is pressed
#define MH_UI_ERROR 2 // red title bar
// A message; lines wrap at 36 characters
void MH_UI_Message(const char* title, const char* text, int flags);

// Helper threads (audio, frame copy) belong on CPU1 when the launcher allows
// it (taskset 0x03): the game then has CPU0 to itself. Returns 0 if not allowed
int MH_PinThreadToCPU1(void);
// Load the MiSTer menu core. Call after MH_Close() when the player quits
void MH_QuitToMenu(void);

#ifdef __cplusplus
}
#endif

#endif

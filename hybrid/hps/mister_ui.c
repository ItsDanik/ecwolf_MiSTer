// Screens every hybrid core shares: a list to pick from (which game to start)
// and a message (what is missing). They are drawn straight to the FPGA core,
// 40x25 characters, and work before the game has set up anything.

#include "mister_hybrid.h"
#include "mister_font.h"

#include <string.h>

#define COLS 40
#define ROWS 25
#define LIST_TOP 5
#define LIST_ROWS 15

enum { C_BACK, C_TEXT, C_TITLE_BACK, C_TITLE_TEXT, C_SELECT_BACK, C_SELECT_TEXT, C_DIM, C_ALERT_BACK };

static const uint32_t ui_palette[256] = {
    [C_BACK] = 0x101420,
    [C_TEXT] = 0xD8D8D8,
    [C_TITLE_BACK] = 0x2848A0,
    [C_TITLE_TEXT] = 0xFFFFFF,
    [C_SELECT_BACK] = 0xE0B030,
    [C_SELECT_TEXT] = 0x101420,
    [C_DIM] = 0x7880A0,
    [C_ALERT_BACK] = 0xA03028,
};

static uint8_t screen[MH_WIDTH * MH_HEIGHT];

// keys of the PS/2 bitmap
#define KEY_UP 0x175
#define KEY_DOWN 0x172
#define KEY_ENTER 0x5A
#define KEY_KP_ENTER 0x15A
#define KEY_SPACE 0x29
#define KEY_ESC 0x76

#define NAV_UP 1
#define NAV_DOWN 2
#define NAV_OK 4
#define NAV_BACK 8

static void fill_rows(int row, int count, int colour) {
    memset(screen + row * 8 * MH_WIDTH, colour, count * 8 * MH_WIDTH);
}

static void draw_text(int col, int row, const char* text, int max, int colour) {
    int i, x, y;

    for (i = 0; text[i] != 0 && i < max && col + i < COLS; i++) {
        int c = (unsigned char)text[i];
        const unsigned char* glyph = mh_font[(c >= 32 && c < 127 ? c : '?') - 32];
        uint8_t* dst = screen + row * 8 * MH_WIDTH + (col + i) * 8;
        for (y = 0; y < 8; y++) {
            for (x = 0; x < 8; x++) {
                if (glyph[y] & (1 << x)) {
                    dst[y * MH_WIDTH + x] = colour;
                }
            }
        }
    }
}

static void draw_centred(int row, const char* text, int colour) {
    int len = strlen(text);
    draw_text(len < COLS ? (COLS - len) / 2 : 0, row, text, COLS, colour);
}

static void draw_frame(const char* title, int title_back, const char* hint) {
    fill_rows(0, ROWS, C_BACK);
    fill_rows(0, 3, title_back);
    draw_centred(1, title, C_TITLE_TEXT);
    if (hint != NULL) {
        draw_centred(ROWS - 2, hint, C_DIM);
    }
}

static void show(void) {
    MH_SetFormat(MH_FORMAT_INDEX8);
    // the picture appears with the palette, so the frame goes first
    MH_Present(screen, MH_WIDTH);
    MH_SetPalette(ui_palette);
}

static int key_down(const mh_input* input, int key) {
    return (input->keys[key >> 5] >> (key & 0x1f)) & 1;
}

static int read_nav(void) {
    mh_input input;
    uint32_t ok_bits, back_bits;
    int nav = 0;

    MH_ReadInput(&input);
    MH_MenuButtons(input.osd_status, &ok_bits, &back_bits);
    if (key_down(&input, KEY_UP) || (input.joystick[0] & MH_JOY_UP) || input.analog_ly[0] < -64) {
        nav |= NAV_UP;
    }
    if (key_down(&input, KEY_DOWN) || (input.joystick[0] & MH_JOY_DOWN) || input.analog_ly[0] > 64) {
        nav |= NAV_DOWN;
    }
    if (key_down(&input, KEY_ESC) || (input.joystick[0] & back_bits)) {
        nav |= NAV_BACK;
    } else if (key_down(&input, KEY_ENTER) || key_down(&input, KEY_KP_ENTER) || key_down(&input, KEY_SPACE)
        || (input.joystick[0] & ~0xFu)) {
        // any button that is not Back confirms
        nav |= NAV_OK;
    }
    return nav;
}

// Newly pressed controls, with auto-repeat for up and down. Returns -1 once
// the core is gone.
static int wait_nav(int* held, int* repeat) {
    int nav, pressed;

    MH_WaitField(MH_FieldCounter());
    if (!MH_CheckAlive()) {
        return -1;
    }
    nav = read_nav();
    pressed = nav & ~*held;
    if (nav != *held) {
        *repeat = 0;
    } else if ((nav & (NAV_UP | NAV_DOWN)) && ++*repeat >= 24) {
        // after 0.4s, 10 steps per second
        *repeat = 18;
        pressed = nav & (NAV_UP | NAV_DOWN);
    }
    *held = nav;
    return pressed;
}

int MH_UI_Menu(const char* title, const char* prompt, const char* const* items, int count, int selected) {
    int held = NAV_UP | NAV_DOWN | NAV_OK | NAV_BACK; // what is held from before does not count
    int repeat = 0;
    int top = 0;
    int pressed;
    int i;

    if (!MH_IsOpen() || count <= 0) {
        return -1;
    }
    selected = selected < 0 || selected >= count ? 0 : selected;
    for (;;) {
        if (selected < top) {
            top = selected;
        } else if (selected >= top + LIST_ROWS) {
            top = selected - LIST_ROWS + 1;
        }
        draw_frame(title, C_TITLE_BACK, "Up/Down: select   Button/Enter: OK");
        if (prompt != NULL) {
            draw_text(2, LIST_TOP - 2, prompt, COLS - 4, C_DIM);
        }
        for (i = top; i < count && i < top + LIST_ROWS; i++) {
            int row = LIST_TOP + i - top;
            if (i == selected) {
                fill_rows(row, 1, C_SELECT_BACK);
            }
            draw_text(2, row, items[i], COLS - 4, i == selected ? C_SELECT_TEXT : C_TEXT);
        }
        if (top > 0) {
            draw_text(COLS - 2, LIST_TOP, "^", 1, C_DIM);
        }
        if (top + LIST_ROWS < count) {
            draw_text(COLS - 2, LIST_TOP + LIST_ROWS - 1, "v", 1, C_DIM);
        }
        show();

        do {
            pressed = wait_nav(&held, &repeat);
        } while (pressed == 0);
        if (pressed < 0 || (pressed & NAV_BACK)) {
            return -1;
        }
        if (pressed & NAV_OK) {
            return selected;
        }
        if (pressed & NAV_UP) {
            selected = selected > 0 ? selected - 1 : count - 1;
        }
        if (pressed & NAV_DOWN) {
            selected = selected < count - 1 ? selected + 1 : 0;
        }
    }
}

void MH_UI_Message(const char* title, const char* text, int flags) {
    int held = NAV_UP | NAV_DOWN | NAV_OK | NAV_BACK;
    int repeat = 0;
    int row = 5;
    int pressed;

    if (!MH_IsOpen()) {
        return;
    }
    draw_frame(title, (flags & MH_UI_ERROR) ? C_ALERT_BACK : C_TITLE_BACK, (flags & MH_UI_WAIT) ? "Press a button" : NULL);
    // word wrap, new lines as given
    while (*text != 0 && row < ROWS - 3) {
        int len = strcspn(text, "\n");
        if (len > COLS - 4) {
            len = COLS - 4;
            while (len > 0 && text[len] != ' ') {
                len--;
            }
            if (len == 0) {
                len = COLS - 4;
            }
        }
        draw_text(2, row++, text, len, C_TEXT);
        text += len;
        if (*text == '\n' || *text == ' ') {
            text++;
        }
    }
    show();
    if (flags & MH_UI_WAIT) {
        do {
            pressed = wait_nav(&held, &repeat);
        } while (pressed >= 0 && !(pressed & (NAV_OK | NAV_BACK)));
    }
}

// HPS side of a MiSTer hybrid core, see mister_hybrid.h.
//
// The FPGA side (hybrid/rtl/hybrid_host.sv) scans out one of three
// framebuffers every field, reloads the palette when its sequence number
// changes and publishes input state once per vblank.
//
// The shared memory is mapped uncached, which makes copying a frame into it
// slow (about 1ms per 64KB). A present thread on CPU1 (when the launcher
// allows it) does that copy so the game thread only copies into a cached
// staging buffer.

#define _GNU_SOURCE
#include "mister_hybrid.h"

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define SHM_PHYS 0x30000000
#define SHM_SIZE 0x400000

#define CTRL_OFFSET 0x0
#define STATUS_OFFSET 0x40
#define AUDIO_PTR_OFFSET 0xC0
#define AUDIO_RING_OFFSET 0x10000
#define PALETTE_OFFSET 0x1000
#define PALETTE_SLOT_SIZE 0x400
#define FB_OFFSET 0x100000
#define FB_SIZE 0x100000
#define FB_COUNT 3

#define CTRL_MAGIC 0x4259484D   // "MHYB"
#define STATUS_MAGIC 0x5359484D // "MHYS"

static int mem_fd = -1;
static volatile uint8_t* shm;
static volatile uint32_t* ctrl;
static volatile uint32_t* status;
static char core_name[64];
static int shm_is_file;

static int fb_format;
static int fb_current;
static int palette_slot;
static uint32_t palette_seq;
static int ctrl_enabled;
static int audio_enabled;
// set once the core is gone: another core may own the memory now, never write to it again
static volatile int detached;

static int cpu1_allowed;

// present thread: FIFO of up to 2 staged frames
static pthread_t present_thread;
static int present_threaded;
static int present_running;
static pthread_mutex_t present_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t present_cond = PTHREAD_COND_INITIALIZER;
static uint8_t* staging[2];
static int staging_next;
static int queue[2];
static int queue_head;
static int queue_count;
// serialises control block updates between the game, present and audio threads
static pthread_mutex_t ctrl_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t alive_frame;
static struct timespec alive_time;

static uint32_t audio_wr;

static void write_ctrl(void) {
    // Palette/framebuffer writes must land before the FPGA can see the new control block
    pthread_mutex_lock(&ctrl_lock);
    __sync_synchronize();
    ctrl[2] = palette_seq;
    ctrl[3] = 0;
    ctrl[1] = fb_current | (fb_format << 8) | (palette_slot << 16) | (audio_enabled << 24);
    ctrl[0] = ctrl_enabled ? CTRL_MAGIC : 0;
    __sync_synchronize();
    pthread_mutex_unlock(&ctrl_lock);
}

static void sleep_ms(int ms) {
    struct timespec ts = { 0, ms * 1000000 };
    nanosleep(&ts, NULL);
}

static int frame_bytes(void) {
    return MH_WIDTH * MH_HEIGHT * (fb_format == MH_FORMAT_RGB565 ? 2 : 1);
}

// 1 if /tmp/CORENAME names our core (or there is nothing to compare)
static int corename_matches(void) {
    char name[64] = { 0 };
    size_t len = strlen(core_name);
    FILE* f;

    if (len == 0 || shm_is_file) {
        return 1;
    }
    f = fopen("/tmp/CORENAME", "r");
    if (f == NULL) {
        return 1;
    }
    if (fgets(name, sizeof(name), f) == NULL) {
        name[0] = 0;
    }
    fclose(f);
    return strncmp(name, core_name, len) == 0;
}

int MH_Open(void) {
    const char* env;
    const char* shm_file = getenv("MISTER_HYBRID_SHM");
    cpu_set_t cpus;
    uint32_t frame;
    int i;

    if (shm != NULL) {
        return !detached;
    }
    env = getenv("MISTER_HYBRID_CORE");
    snprintf(core_name, sizeof(core_name), "%s", env ? env : "");

    shm_is_file = shm_file != NULL && shm_file[0] != 0;
    mem_fd = open(shm_is_file ? shm_file : "/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        fprintf(stderr, "mister: cannot open %s, running headless\n", shm_is_file ? shm_file : "/dev/mem");
        return 0;
    }
    shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, shm_is_file ? 0 : SHM_PHYS);
    if (shm == MAP_FAILED) {
        fprintf(stderr, "mister: cannot map shared memory, running headless\n");
        close(mem_fd);
        mem_fd = -1;
        shm = NULL;
        return 0;
    }
    ctrl = (volatile uint32_t*)(shm + CTRL_OFFSET);
    status = (volatile uint32_t*)(shm + STATUS_OFFSET);

    // The core rewrites the status block every vblank; a stale block from an
    // earlier session has a frozen frame counter.
    frame = status[1];
    for (i = 0; i < 20 && status[1] == frame; i++) {
        sleep_ms(10);
    }
    if (status[0] != STATUS_MAGIC || status[1] == frame || !corename_matches()) {
        fprintf(stderr, "mister: %s core not running, running headless\n", core_name[0] ? core_name : "hybrid");
        munmap((void*)shm, SHM_SIZE);
        shm = NULL;
        close(mem_fd);
        mem_fd = -1;
        return 0;
    }
    detached = 0;
    ctrl_enabled = 0;
    audio_enabled = 0;
    alive_time.tv_sec = 0;
    for (i = 0; i < FB_COUNT; i++) {
        memset((void*)(shm + FB_OFFSET + i * FB_SIZE), 0, MH_WIDTH * MH_HEIGHT * 2);
    }
    write_ctrl();

    // With both CPUs allowed (taskset 0x03 in the launcher) the game thread
    // gets CPU0 to itself: it has the better DDR3 bandwidth and Main_MiSTer
    // lives on CPU1. Helper threads go to CPU1 (MH_PinThreadToCPU1).
    cpu1_allowed = 0;
    if (sched_getaffinity(0, sizeof(cpus), &cpus) == 0 && CPU_ISSET(0, &cpus) && CPU_ISSET(1, &cpus)) {
        cpu1_allowed = 1;
        CPU_ZERO(&cpus);
        CPU_SET(0, &cpus);
        sched_setaffinity(0, sizeof(cpus), &cpus);
    }
    printf("mister: %s core found (host version %u)%s\n", core_name[0] ? core_name : "hybrid", status[3],
        cpu1_allowed ? ", helper threads on CPU1" : "");
    return 1;
}

int MH_PinThreadToCPU1(void) {
    cpu_set_t cpu1;

    if (!cpu1_allowed) {
        return 0;
    }
    CPU_ZERO(&cpu1);
    CPU_SET(1, &cpu1);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu1), &cpu1) == 0;
}

static void stop_present_thread(void) {
    if (!present_threaded) {
        return;
    }
    pthread_mutex_lock(&present_lock);
    present_running = 0;
    pthread_cond_broadcast(&present_cond);
    pthread_mutex_unlock(&present_lock);
    pthread_join(present_thread, NULL);
    present_threaded = 0;
    queue_count = 0;
}

void MH_Close(void) {
    stop_present_thread();
    if (shm != NULL) {
        if (!detached && corename_matches()) {
            // back to the core's test pattern, audio off. Not once another
            // core is loaded: the memory is its own then
            ctrl_enabled = 0;
            audio_enabled = 0;
            write_ctrl();
        }
        munmap((void*)shm, SHM_SIZE);
        shm = NULL;
    }
    if (mem_fd >= 0) {
        close(mem_fd);
        mem_fd = -1;
    }
}

int MH_IsOpen(void) {
    return shm != NULL && !detached;
}

const char* MH_CoreName(void) {
    return core_name;
}

// Copy a frame into the next framebuffer and flip to it
static void present_to_fpga(const uint8_t* pixels, int pitch) {
    volatile uint8_t* dst;
    int row = frame_bytes() / MH_HEIGHT;
    int next;
    int y;
    int i;

    // Triple buffering. Never write into the buffer that is being scanned out:
    // with two frames submitted within one vblank the FPGA may still show it.
    next = (fb_current + 1) % FB_COUNT;
    for (i = 0; i < 50 && (status[2] & 0xff) == (uint32_t)next; i++) {
        sleep_ms(1);
    }

    dst = shm + FB_OFFSET + next * FB_SIZE;
    if (pitch == row) {
        memcpy((void*)dst, pixels, row * MH_HEIGHT);
    } else {
        for (y = 0; y < MH_HEIGHT; y++) {
            memcpy((void*)(dst + y * row), pixels + y * pitch, row);
        }
    }
    fb_current = next;
    if (fb_format == MH_FORMAT_RGB565) {
        ctrl_enabled = 1;
    }
    write_ctrl();
}

static void* present_thread_main(void* arg) {
    int slot;

    (void)arg;
    MH_PinThreadToCPU1();
    pthread_mutex_lock(&present_lock);
    for (;;) {
        while (queue_count == 0 && present_running) {
            pthread_cond_wait(&present_cond, &present_lock);
        }
        if (queue_count == 0) {
            break;
        }
        slot = queue[queue_head];
        pthread_mutex_unlock(&present_lock);

        if (MH_IsOpen()) {
            present_to_fpga(staging[slot], frame_bytes() / MH_HEIGHT);
        }

        pthread_mutex_lock(&present_lock);
        queue_head = (queue_head + 1) % 2;
        queue_count--;
        pthread_cond_broadcast(&present_cond);
    }
    pthread_mutex_unlock(&present_lock);
    return NULL;
}

static void start_present_thread(void) {
    // only worth it with a second CPU to run on
    if (present_threaded || !cpu1_allowed) {
        return;
    }
    if (staging[0] == NULL) {
        staging[0] = malloc(MH_WIDTH * MH_HEIGHT * 2);
        staging[1] = malloc(MH_WIDTH * MH_HEIGHT * 2);
    }
    present_running = 1;
    if (pthread_create(&present_thread, NULL, present_thread_main, NULL) == 0) {
        present_threaded = 1;
    } else {
        present_running = 0;
    }
}

// Wait until all staged frames reached the FPGA
static void present_flush(void) {
    if (!present_threaded) {
        return;
    }
    pthread_mutex_lock(&present_lock);
    while (queue_count > 0) {
        pthread_cond_wait(&present_cond, &present_lock);
    }
    pthread_mutex_unlock(&present_lock);
}

void MH_SetFormat(int format) {
    if (!MH_IsOpen() || format == fb_format) {
        return;
    }
    present_flush();
    fb_format = format;
    // the test pattern stays up until the first frame (and palette) in the new format
    ctrl_enabled = 0;
    write_ctrl();
}

void MH_Present(const void* pixels, int pitch) {
    const uint8_t* src = pixels;
    uint8_t* dst;
    int row = frame_bytes() / MH_HEIGHT;
    int slot;
    int y;

    if (!MH_IsOpen()) {
        return;
    }
    start_present_thread();
    if (!present_threaded) {
        present_to_fpga(src, pitch);
        return;
    }

    pthread_mutex_lock(&present_lock);
    while (queue_count == 2) {
        pthread_cond_wait(&present_cond, &present_lock);
    }
    slot = staging_next;
    staging_next ^= 1;
    pthread_mutex_unlock(&present_lock);

    dst = staging[slot];
    if (pitch == row) {
        memcpy(dst, src, row * MH_HEIGHT);
    } else {
        for (y = 0; y < MH_HEIGHT; y++) {
            memcpy(dst + y * row, src + y * pitch, row);
        }
    }

    pthread_mutex_lock(&present_lock);
    queue[(queue_head + queue_count) % 2] = slot;
    queue_count++;
    pthread_cond_signal(&present_cond);
    pthread_mutex_unlock(&present_lock);
}

void MH_SetPalette(const uint32_t* rgb) {
    volatile uint32_t* pal;
    int i;

    if (!MH_IsOpen()) {
        return;
    }
    // Frames queued before this palette change must reach the FPGA first
    present_flush();

    // Two palette slots: fill the one the FPGA is not using, then switch
    palette_slot ^= 1;
    pal = (volatile uint32_t*)(shm + PALETTE_OFFSET + palette_slot * PALETTE_SLOT_SIZE);
    for (i = 0; i < 256; i++) {
        pal[i] = rgb[i] & 0xffffff;
    }
    palette_seq++;
    ctrl_enabled = 1;
    write_ctrl();
}

uint32_t MH_FieldCounter(void) {
    return MH_IsOpen() ? status[1] : 0;
}

void MH_WaitField(uint32_t field) {
    int i;

    // at most 4 fields, in case the core goes away
    for (i = 0; i < 70 && MH_IsOpen() && (int32_t)(status[1] - field) <= 0; i++) {
        sleep_ms(1);
    }
}

static long elapsed_ms(const struct timespec* since) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - since->tv_sec) * 1000 + (now.tv_nsec - since->tv_nsec) / 1000000;
}

int MH_CheckAlive(void) {
    if (!MH_IsOpen()) {
        return !detached;
    }
    if (alive_time.tv_sec == 0) {
        alive_frame = status[1];
        clock_gettime(CLOCK_MONOTONIC, &alive_time);
        return 1;
    }
    if (elapsed_ms(&alive_time) < 500) {
        return 1;
    }
    // Another core may use the same memory: stop touching it as soon as ours is gone
    if (status[0] != STATUS_MAGIC || status[1] == alive_frame || !corename_matches()) {
        detached = 1;
        return 0;
    }
    alive_frame = status[1];
    clock_gettime(CLOCK_MONOTONIC, &alive_time);
    return 1;
}

void MH_ReadInput(mh_input* input) {
    int i;

    memset(input, 0, sizeof(*input));
    if (!MH_IsOpen()) {
        return;
    }
    input->frame = status[1];
    input->joystick[0] = status[4];
    input->joystick[1] = status[5];
    for (i = 0; i < 2; i++) {
        uint32_t analog = status[6 + i];
        input->analog_lx[i] = (int8_t)(analog & 0xff);
        input->analog_ly[i] = (int8_t)((analog >> 8) & 0xff);
        input->analog_rx[i] = (int8_t)((analog >> 16) & 0xff);
        input->analog_ry[i] = (int8_t)((analog >> 24) & 0xff);
    }
    input->osd_status = status[8] | ((uint64_t)status[9] << 32);
    input->mouse_x = (int32_t)status[10];
    input->mouse_y = (int32_t)status[11];
    input->mouse_buttons = status[12] & 7;
    input->mouse_wheel = (int16_t)(status[12] >> 16);
    for (i = 0; i < 16; i++) {
        input->keys[i] = status[16 + i];
    }
}

uint64_t MH_OSDStatus(void) {
    if (!MH_IsOpen()) {
        return 0;
    }
    return status[8] | ((uint64_t)status[9] << 32);
}

void MH_AudioEnable(int enabled) {
    volatile uint32_t* ring;
    int i;

    if (!MH_IsOpen() || enabled == audio_enabled) {
        return;
    }
    if (enabled) {
        // the core starts fetching at frame 0 of the ring at the next vblank
        ring = (volatile uint32_t*)(shm + AUDIO_RING_OFFSET);
        for (i = 0; i < MH_AUDIO_RING_FRAMES; i++) {
            ring[i] = 0;
        }
        *(volatile uint32_t*)(shm + AUDIO_PTR_OFFSET) = 0;
        audio_wr = 0;
    }
    audio_enabled = enabled;
    write_ctrl();
}

void MH_AudioWrite(const uint32_t* frames, int count, int lead) {
    volatile uint32_t* ring;
    volatile uint32_t* fetch_ptr;
    int32_t ahead;
    int i;

    if (!MH_IsOpen() || !audio_enabled) {
        // keep the caller's pace without the core
        struct timespec ts = { 0, (long)count * 1000000000LL / MH_AUDIO_RATE };
        nanosleep(&ts, NULL);
        return;
    }
    ring = (volatile uint32_t*)(shm + AUDIO_RING_OFFSET);
    fetch_ptr = (volatile uint32_t*)(shm + AUDIO_PTR_OFFSET);

    // the core's sample clock paces us
    for (i = 0; i < 1000 && MH_IsOpen(); i++) {
        ahead = (int32_t)(audio_wr - *fetch_ptr);
        if (ahead <= lead) {
            break;
        }
        sleep_ms(1);
    }
    if (!MH_IsOpen()) {
        return;
    }
    ahead = (int32_t)(audio_wr - *fetch_ptr);
    if (ahead < 0 || ahead > MH_AUDIO_RING_FRAMES - count) {
        // fell behind (the core repeats its last sample meanwhile): skip ahead
        audio_wr = *fetch_ptr + 64;
    }
    for (i = 0; i < count; i++) {
        ring[(audio_wr + i) % MH_AUDIO_RING_FRAMES] = frames[i];
    }
    audio_wr += count;
}

void MH_QuitToMenu(void) {
    FILE* f;

    if (shm_is_file) {
        return;
    }
    f = fopen("/dev/MiSTer_cmd", "w");
    if (f != NULL) {
        fputs("load_core /media/fat/menu.rbf\n", f);
        fclose(f);
    }
}

// PS/2 set 2 -> USB HID usage
static const uint8_t set2_normal[0x84] = {
    [0x1C] = 0x04, [0x32] = 0x05, [0x21] = 0x06, [0x23] = 0x07, [0x24] = 0x08, [0x2B] = 0x09, // A-F
    [0x34] = 0x0A, [0x33] = 0x0B, [0x43] = 0x0C, [0x3B] = 0x0D, [0x42] = 0x0E, [0x4B] = 0x0F, // G-L
    [0x3A] = 0x10, [0x31] = 0x11, [0x44] = 0x12, [0x4D] = 0x13, [0x15] = 0x14, [0x2D] = 0x15, // M-R
    [0x1B] = 0x16, [0x2C] = 0x17, [0x3C] = 0x18, [0x2A] = 0x19, [0x1D] = 0x1A, [0x22] = 0x1B, // S-X
    [0x35] = 0x1C, [0x1A] = 0x1D,                                                             // Y, Z
    [0x16] = 0x1E, [0x1E] = 0x1F, [0x26] = 0x20, [0x25] = 0x21, [0x2E] = 0x22,                // 1-5
    [0x36] = 0x23, [0x3D] = 0x24, [0x3E] = 0x25, [0x46] = 0x26, [0x45] = 0x27,                // 6-0
    [0x5A] = 0x28, // Enter
    [0x76] = 0x29, // Escape
    [0x66] = 0x2A, // Backspace
    [0x0D] = 0x2B, // Tab
    [0x29] = 0x2C, // Space
    [0x4E] = 0x2D, // -
    [0x55] = 0x2E, // =
    [0x54] = 0x2F, // [
    [0x5B] = 0x30, // ]
    [0x5D] = 0x31, // backslash
    [0x4C] = 0x33, // ;
    [0x52] = 0x34, // '
    [0x0E] = 0x35, // `
    [0x41] = 0x36, // ,
    [0x49] = 0x37, // .
    [0x4A] = 0x38, // /
    [0x58] = 0x39, // Caps Lock
    [0x05] = 0x3A, [0x06] = 0x3B, [0x04] = 0x3C, [0x0C] = 0x3D, [0x03] = 0x3E, [0x0B] = 0x3F, // F1-F6
    [0x83] = 0x40, [0x0A] = 0x41, [0x01] = 0x42, [0x09] = 0x43, [0x78] = 0x44, [0x07] = 0x45, // F7-F12
    [0x7E] = 0x47, // Scroll Lock
    [0x77] = 0x53, // Num Lock
    [0x7C] = 0x55, // keypad *
    [0x7B] = 0x56, // keypad -
    [0x79] = 0x57, // keypad +
    [0x69] = 0x59, [0x72] = 0x5A, [0x7A] = 0x5B, [0x6B] = 0x5C, [0x73] = 0x5D, // keypad 1-5
    [0x74] = 0x5E, [0x6C] = 0x5F, [0x75] = 0x60, [0x7D] = 0x61, [0x70] = 0x62, // keypad 6-0
    [0x71] = 0x63, // keypad .
    [0x61] = 0x64, // non-US backslash
    [0x14] = 0xE0, // left Ctrl
    [0x12] = 0xE1, // left Shift
    [0x11] = 0xE2, // left Alt
    [0x59] = 0xE5, // right Shift
};

// E0-prefixed PS/2 set 2 codes
static const uint8_t set2_extended[0x80] = {
    [0x7C] = 0x46, // Print Screen
    [0x70] = 0x49, // Insert
    [0x6C] = 0x4A, // Home
    [0x7D] = 0x4B, // Page Up
    [0x71] = 0x4C, // Delete
    [0x69] = 0x4D, // End
    [0x7A] = 0x4E, // Page Down
    [0x74] = 0x4F, // Right
    [0x6B] = 0x50, // Left
    [0x72] = 0x51, // Down
    [0x75] = 0x52, // Up
    [0x4A] = 0x54, // keypad /
    [0x5A] = 0x58, // keypad Enter
    [0x2F] = 0x65, // Application
    [0x1F] = 0xE3, // left GUI
    [0x14] = 0xE4, // right Ctrl
    [0x11] = 0xE6, // right Alt
    [0x27] = 0xE7, // right GUI
};

int MH_KeyToHID(int key_index) {
    int code = key_index & 0xff;

    if (key_index & 0x100) {
        return code < 0x80 ? set2_extended[code] : 0;
    }
    return code < 0x84 ? set2_normal[code] : 0;
}

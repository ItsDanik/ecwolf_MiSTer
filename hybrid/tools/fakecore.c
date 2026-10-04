// Stand-in for the FPGA core, to run a hybrid game on the PC.
//
// It keeps the shared memory of hybrid/rtl/hybrid_host.sv in a file: the
// status block and field counter tick at the core's 59.6Hz, the audio ring is
// consumed at 44.1kHz. A script feeds input and takes screenshots. Start it
// first, then the game with MISTER_HYBRID_SHM=<file>.
//
// usage: fakecore <shm file> [script] [audio.raw]
//   audio.raw receives what the game plays (16-bit stereo, 44.1kHz)
// Script, one command per line (# starts a comment):
//   wait <fields>           let time pass
//   key <index> <0|1>       release/press a key, index = PS/2 set 2 code in hex,
//                           +100 for E0-prefixed codes (5A Enter, 76 Esc, 175 up)
//   joy <hex>               joystick 1 bits (1 right, 2 left, 4 down, 8 up, 10.. buttons)
//   analog <x> <y> [rx ry]  left (and right) stick of joystick 1, -128..127
//   mouse <dx> <dy> <btn>   move the mouse (y up) and set its buttons
//   osd <hex>               OSD status bits
//   shot <file.ppm>         save the picture the core would show
//   quit                    stop ticking, as if another core was loaded
// Without a script (or after it ends) it runs until killed.

#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define SHM_SIZE 0x400000
#define CTRL_MAGIC 0x4259484D
#define STATUS_MAGIC 0x5359484D
#define FIELD_NS 16768000 // 400 x 262 pixels at 6.25MHz

static volatile uint8_t* shm;
static volatile uint32_t* ctrl;
static volatile uint32_t* status;
static uint32_t field;
static uint32_t palette[256];
static uint32_t palette_seq = 0xffffffff;
static int fb_index, fb_format, ctrl_valid;
static uint32_t audio_fetch;
static uint64_t audio_frac;
static FILE* audio_out;

static void tick(void) {
    static struct timespec next;
    uint32_t c0 = ctrl[0], c1 = ctrl[1], c2 = ctrl[2];
    int audio_en;

    if (next.tv_sec == 0) {
        clock_gettime(CLOCK_MONOTONIC, &next);
    }
    next.tv_nsec += FIELD_NS;
    if (next.tv_nsec >= 1000000000) {
        next.tv_nsec -= 1000000000;
        next.tv_sec++;
    }
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

    // vblank: latch the control block like the core does
    ctrl_valid = c0 == CTRL_MAGIC;
    audio_en = ctrl_valid && ((c1 >> 24) & 1);
    if (ctrl_valid) {
        fb_index = (c1 & 0xff) > 2 ? 0 : (c1 & 3);
        fb_format = (c1 >> 8) & 1;
        if (c2 != palette_seq) {
            palette_seq = c2;
            memcpy(palette, (void*)(shm + 0x1000 + ((c1 >> 16) & 1) * 0x400), sizeof(palette));
        }
    }
    if (!audio_en) {
        audio_fetch = 0;
        audio_frac = 0;
    } else {
        // 44100 frames per second, fetched in bursts of 32
        uint32_t target;
        audio_frac += 44100ull * FIELD_NS;
        target = (uint32_t)(audio_frac / 1000000000ull);
        while (audio_fetch + 32 <= target + 128) {
            if (audio_out != NULL) {
                fwrite((void*)(shm + 0x10000 + (audio_fetch % 16384) * 4), 4, 32, audio_out);
            }
            audio_fetch += 32;
            *(volatile uint32_t*)(shm + 0xC0) = audio_fetch;
        }
    }
    field++;
    status[2] = fb_index | (fb_format << 8) | (ctrl_valid << 10);
    status[3] = 2;
    status[0] = STATUS_MAGIC;
    status[1] = field;
}

static void screenshot(const char* path) {
    const volatile uint8_t* fb = shm + 0x100000 + fb_index * 0x100000;
    FILE* f = fopen(path, "wb");
    int i;

    if (f == NULL) {
        perror(path);
        return;
    }
    fprintf(f, "P6\n320 200\n255\n");
    for (i = 0; i < 320 * 200; i++) {
        uint8_t rgb[3] = { 0, 0, 0 };
        if (!ctrl_valid) {
            // the core's colour bars
            int bar = (i % 320) >> 6;
            rgb[0] = (bar & 2) ? 0xC0 : 0;
            rgb[1] = (bar & 4) ? 0xC0 : 0;
            rgb[2] = (bar & 1) ? 0xC0 : 0;
        } else if (fb_format) {
            uint16_t p = fb[i * 2] | (fb[i * 2 + 1] << 8);
            rgb[0] = (p >> 11) << 3 | (p >> 13);
            rgb[1] = ((p >> 5) & 0x3f) << 2 | ((p >> 9) & 3);
            rgb[2] = (p & 0x1f) << 3 | ((p >> 2) & 7);
        } else {
            uint32_t c = palette[fb[i]];
            rgb[0] = c >> 16;
            rgb[1] = c >> 8;
            rgb[2] = c;
        }
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    FILE* script = NULL;
    char line[256];
    int fd;

    if (argc < 2) {
        fprintf(stderr, "usage: fakecore <shm file> [script] [audio.raw]\n");
        return 1;
    }
    fd = open(argv[1], O_RDWR | O_CREAT, 0644);
    if (fd < 0 || ftruncate(fd, SHM_SIZE) != 0) {
        perror(argv[1]);
        return 1;
    }
    shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    ctrl = (volatile uint32_t*)shm;
    status = (volatile uint32_t*)(shm + 0x40);
    memset((void*)shm, 0, 0x1000);
    if (argc > 2 && (script = fopen(argv[2], "r")) == NULL) {
        perror(argv[2]);
        return 1;
    }
    if (argc > 3) {
        audio_out = fopen(argv[3], "wb");
    }

    while (script != NULL && fgets(line, sizeof(line), script) != NULL) {
        char arg[200];
        int a, b, c = 0, d = 0;

        if (sscanf(line, "wait %d", &a) == 1) {
            while (a-- > 0) {
                tick();
            }
        } else if (sscanf(line, "key %x %d", &a, &b) == 2) {
            if (b) {
                status[16 + ((a & 0x1ff) >> 5)] |= 1u << (a & 0x1f);
            } else {
                status[16 + ((a & 0x1ff) >> 5)] &= ~(1u << (a & 0x1f));
            }
        } else if (sscanf(line, "joy %x", &a) == 1) {
            status[4] = a;
        } else if (sscanf(line, "analog %d %d %d %d", &a, &b, &c, &d) >= 2) {
            status[6] = (a & 0xff) | ((b & 0xff) << 8) | ((c & 0xff) << 16) | ((d & 0xff) << 24);
        } else if (sscanf(line, "mouse %d %d %d", &a, &b, &c) == 3) {
            status[10] += a;
            status[11] += b;
            status[12] = (status[12] & ~7u) | (c & 7);
        } else if (sscanf(line, "osd %x", &a) == 1) {
            status[8] = a;
        } else if (sscanf(line, "shot %199s", arg) == 1) {
            screenshot(arg);
        } else if (strncmp(line, "quit", 4) == 0) {
            break;
        }
    }
    if (script == NULL || feof(script)) {
        for (;;) {
            tick();
        }
    }
    if (audio_out != NULL) {
        fclose(audio_out);
    }
    return 0;
}

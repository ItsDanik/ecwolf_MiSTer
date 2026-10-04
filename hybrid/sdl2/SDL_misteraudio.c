/*
  SDL audio driver for MiSTer hybrid cores.

  The FPGA core plays 44.1kHz 16-bit stereo from a ring in shared memory
  (hybrid/rtl/hybrid_host.sv) and its sample clock paces the audio thread.
  Other formats and rates are converted by SDL. See mister_hybrid.h.

  Hint (also read from the environment):
    SDL_MISTER_AUDIO_LEAD  frames kept queued ahead of the core on top of the
                           device buffer (default 512, about 12ms)

  This software is provided 'as-is' under the zlib license, like SDL itself.
*/
#include "../../SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_MISTER

#include "SDL_audio.h"
#include "SDL_hints.h"
#include "SDL_thread.h"
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"

#include "mister_hybrid.h"

#define _THIS SDL_AudioDevice *_this

struct SDL_PrivateAudioData
{
    Uint8 *mixbuf;
    int lead;
};

static void MISTERAUDIO_CloseDevice(_THIS)
{
    MH_AudioEnable(0);
    if (_this->hidden) {
        SDL_free(_this->hidden->mixbuf);
        SDL_free(_this->hidden);
        _this->hidden = NULL;
    }
}

static int MISTERAUDIO_OpenDevice(_THIS, const char *devname)
{
    const char *hint = SDL_GetHint("SDL_MISTER_AUDIO_LEAD");

    (void)devname;
    _this->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*_this->hidden));
    if (!_this->hidden) {
        return SDL_OutOfMemory();
    }

    /* what the core plays; SDL converts if the application wants something else */
    _this->spec.freq = MH_AUDIO_RATE;
    _this->spec.format = AUDIO_S16LSB;
    _this->spec.channels = 2;
    if (_this->spec.samples > MH_AUDIO_RING_FRAMES / 4) {
        _this->spec.samples = MH_AUDIO_RING_FRAMES / 4;
    }
    SDL_CalculateAudioSpec(&_this->spec);

    _this->hidden->lead = hint ? SDL_atoi(hint) : 512;
    if (_this->hidden->lead < 64) {
        _this->hidden->lead = 64;
    }
    _this->hidden->mixbuf = (Uint8 *)SDL_calloc(1, _this->spec.size);
    if (!_this->hidden->mixbuf) {
        return SDL_OutOfMemory();
    }
    MH_AudioEnable(1);
    return 0;
}

static void MISTERAUDIO_ThreadInit(_THIS)
{
    (void)_this;
    /* next to the game thread, which has CPU0 */
    MH_PinThreadToCPU1();
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
}

static void MISTERAUDIO_PlayDevice(_THIS)
{
    /* blocks until the core needs the buffer */
    MH_AudioWrite((const uint32_t *)_this->hidden->mixbuf, _this->spec.samples, _this->hidden->lead);
}

static Uint8 *MISTERAUDIO_GetDeviceBuf(_THIS)
{
    return _this->hidden->mixbuf;
}

static SDL_bool MISTERAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    /* only with the core loaded: SDL tries the next driver otherwise */
    if (!MH_Open()) {
        return SDL_FALSE;
    }

    impl->OpenDevice = MISTERAUDIO_OpenDevice;
    impl->ThreadInit = MISTERAUDIO_ThreadInit;
    impl->PlayDevice = MISTERAUDIO_PlayDevice;
    impl->GetDeviceBuf = MISTERAUDIO_GetDeviceBuf;
    impl->CloseDevice = MISTERAUDIO_CloseDevice;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->SupportsNonPow2Samples = SDL_TRUE;

    return SDL_TRUE;
}

AudioBootStrap MISTERAUDIO_bootstrap = {
    "mister", "MiSTer hybrid core audio driver", MISTERAUDIO_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_MISTER */

/* vi: set ts=4 sw=4 expandtab: */

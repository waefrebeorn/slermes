/* Slermes Windows compat: <alsa/asoundlib.h> — ALSA does not exist on Windows.
 * Audio capture is unavailable; the port's existing "no device" path applies. */
#ifndef SLERMES_COMPAT_ALSA_ASOUNDLIB_H
#define SLERMES_COMPAT_ALSA_ASOUNDLIB_H
#include <stddef.h>
#include <stdint.h>

typedef struct _snd_pcm snd_pcm_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
typedef struct _snd_mixer snd_mixer_t;
typedef unsigned long snd_pcm_uframes_t;
typedef long snd_pcm_sframes_t;
typedef int snd_pcm_stream_t;
typedef int snd_pcm_access_t;
typedef int snd_pcm_format_t;

enum { SND_PCM_STREAM_PLAYBACK = 0, SND_PCM_STREAM_CAPTURE = 1 };
enum { SND_PCM_ACCESS_RW_INTERLEAVED = 3, SND_PCM_ACCESS_RW_NONINTERLEAVED = 4 };
enum { SND_PCM_FORMAT_S16_LE = 2, SND_PCM_FORMAT_S16_BE = 3, SND_PCM_FORMAT_FLOAT_LE = 14 };

#define SND_PCM_NONBLOCK 0x0001
#define SND_PCM_ASYNC    0x0002
#endif

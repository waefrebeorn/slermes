/* Slermes Windows compat: <alsa/asoundlib.h> — ALSA does not exist on Windows.
 * Audio capture is unavailable; the port's existing "no device" path applies. */
#ifndef SLERMES_COMPAT_ALSA_ASOUNDLIB_H
#define SLERMES_COMPAT_ALSA_ASOUNDLIB_H
#include <stddef.h>
#include <stdint.h>

typedef struct _snd_pcm snd_pcm_t;
typedef struct _snd_ctl snd_ctl_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
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
#include <errno.h>
static inline int snd_pcm_open(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode)
{ (void)pcm; (void)name; (void)stream; (void)mode; errno = ENODEV; return -1; }
/* Remaining symbols used by wake_word.c / voice_mode.c. All fail closed. */
typedef struct _snd_ctl_card_info snd_ctl_card_info_t;
static inline int snd_ctl_card_info_alloca(snd_ctl_card_info_t **p) { (void)p; return 0; }
static inline int snd_ctl_card_info(snd_ctl_t *ctl, snd_ctl_card_info_t *info) { (void)ctl; (void)info; return -1; }
static inline int snd_ctl_card_info_get_card(const snd_ctl_card_info_t *i) { (void)i; return -1; }
static inline const char *snd_ctl_card_info_get_driver(const snd_ctl_card_info_t *i) { (void)i; return NULL; }
static inline const char *snd_ctl_card_info_get_name(const snd_ctl_card_info_t *i) { (void)i; return NULL; }
static inline int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params) { (void)pcm; (void)params; return -1; }
static inline int snd_pcm_hw_params_free(snd_pcm_hw_params_t *p) { (void)p; return 0; }
static inline int snd_pcm_hw_params_get_channels(const snd_pcm_hw_params_t *p, unsigned int *v) { (void)p; if (v) *v = 0; return -1; }
static inline int snd_pcm_hw_params_get_channels_max(const snd_pcm_hw_params_t *p, unsigned int *v) { (void)p; if (v) *v = 0; return -1; }
static inline int snd_pcm_hw_params_get_rate(const snd_pcm_hw_params_t *p, unsigned int *v, int *d) { (void)p; if (v) *v = 0; if (d) *d = 0; return -1; }

/* control interface + hw_params helpers used by wake_word.c */
static inline int snd_ctl_open(snd_ctl_t **ctl, const char *name, int mode)
{ (void)ctl; (void)name; (void)mode; errno = ENODEV; return -1; }
static inline int snd_ctl_close(snd_ctl_t *ctl) { (void)ctl; return 0; }
static inline int snd_pcm_hw_params_alloca(snd_pcm_hw_params_t **p) { (void)p; return 0; }

static inline int snd_pcm_close(snd_pcm_t *pcm) { (void)pcm; return 0; }
static inline int snd_pcm_set_params(snd_pcm_t *pcm, snd_pcm_format_t fmt, snd_pcm_access_t acc,
                                     unsigned int channels, unsigned int rate, int soft_resample,
                                     unsigned int latency)
{ (void)pcm; (void)fmt; (void)acc; (void)channels; (void)rate; (void)soft_resample; (void)latency; return -1; }
static inline snd_pcm_sframes_t snd_pcm_readi(snd_pcm_t *pcm, void *buf, snd_pcm_uframes_t n)
{ (void)pcm; (void)buf; (void)n; return -1; }
static inline int snd_pcm_recover(snd_pcm_t *pcm, int err, int silent)
{ (void)pcm; (void)err; (void)silent; return -1; }
static inline int snd_pcm_prepare(snd_pcm_t *pcm) { (void)pcm; return -1; }
static inline int snd_pcm_drain(snd_pcm_t *pcm) { (void)pcm; return -1; }
static inline const char *snd_strerror(int errnum) { (void)errnum; return "ALSA unavailable on Windows"; }
#endif

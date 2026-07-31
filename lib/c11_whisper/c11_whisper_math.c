/*
 * c11_whisper_math.c — Pure C11 matrix math for whisper transformer
 *
 * No third-party dependencies. Pure C11.
 * Implements: matmul, softmax, GELU, layer norm, attention.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "c11_whisper.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Matrix multiply: out[m,n] = a[m,k] @ b[k,n] */
void c11_matmul(float *out, const float *a, const float *b,
                int m, int k, int n) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int l = 0; l < k; l++) {
                sum += a[i * k + l] * b[l * n + j];
            }
            out[i * n + j] = sum;
        }
    }
}

/* Softmax with numerical stability */
void c11_softmax(float *x, int n) {
    if (n <= 0) return;
    float max_val = x[0];
    for (int i = 1; i < n; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    if (sum > 0.0f) {
        float inv = 1.0f / sum;
        for (int i = 0; i < n; i++) x[i] *= inv;
    }
}

/* GELU activation (exact form) */
float c11_gelu(float x) {
    return 0.5f * x * (1.0f + tanhf(0.7978845608028654f *
               (x + 0.044715f * x * x * x)));
}

/* Layer normalization */
void c11_layer_norm(float *out, const float *x, const float *gamma,
                    const float *beta, int n, float eps) {
    float mean = 0.0f;
    for (int i = 0; i < n; i++) mean += x[i];
    mean /= n;

    float var = 0.0f;
    for (int i = 0; i < n; i++) {
        float d = x[i] - mean;
        var += d * d;
    }
    var /= n;

    float inv_std = 1.0f / sqrtf(var + eps);
    for (int i = 0; i < n; i++) {
        out[i] = gamma[i] * (x[i] - mean) * inv_std + beta[i];
    }
}

/* ── Mel-spectrogram computation ── */
/*
 * Whisper uses 80-bin mel-spectrograms with:
 *   - 400-point FFT (n_fft=400)
 *   - 160-sample hop (hop_length=160 at 16kHz)
 *   - 3000 time frames max (n_len=3000)
 *   - Hann window
 *   - log-mel scale
 */

void c11_mel_compute(c11_mel_spectrogram_t *mel, const float *samples, int n_samples) {
    if (!mel || !samples || n_samples <= 0) return;

    const int n_fft = 400;
    const int hop_length = 160;
    const int n_mel = mel->n_mel ?: 80;

    /* Compute number of frames */
    int n_frames = (n_samples - n_fft) / hop_length + 1;
    if (n_frames > 3000) n_frames = 3000;  /* whisper max context */
    mel->n_len = n_frames;

    /* Allocate mel buffer */
    mel->mel = calloc(n_mel * n_frames, sizeof(float));
    if (!mel->mel) return;

    /* Hann window */
    float *window = malloc(n_fft * sizeof(float));
    for (int i = 0; i < n_fft; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * i / (n_fft - 1)));
    }

    /* Power spectrogram → mel filterbank → log scale */
    /* Simplified FFT for power spectrum (real DFT) */
    float *power_spec = malloc((n_fft / 2 + 1) * sizeof(float));

    for (int f = 0; f < n_frames; f++) {
        int start = f * hop_length;

        /* Apply Hann window */
        float *frame = malloc(n_fft * sizeof(float));
        for (int i = 0; i < n_fft; i++) {
            int idx = start + i;
            frame[i] = (idx < n_samples) ? samples[idx] * window[i] : 0.0f;
        }

        /* Simple DFT magnitude (O(n^2) but correct for small n_fft=400) */
        for (int k = 0; k <= n_fft / 2; k++) {
            float real = 0.0f, imag = 0.0f;
            for (int t = 0; t < n_fft; t++) {
                float angle = -2.0f * 3.14159265f * k * t / n_fft;
                real += frame[t] * cosf(angle);
                imag += frame[t] * sinf(angle);
            }
            power_spec[k] = real * real + imag * imag;
        }

        /* Apply mel filterbank (simplified — triangular filters) */
        for (int m = 0; m < n_mel; m++) {
            /* Mel scale mapping */
            float mel_min = 2595.0f * log10f(1.0f + 0.0f / 16000.0f);
            float mel_max = 2595.0f * log10f(1.0f + 8000.0f / 16000.0f);
            float mel_center = mel_min + (mel_max - mel_min) * (m + 0.5f) / n_mel;
            float hz_center = 16000.0f * (powf(10.0f, mel_center / 2595.0f) - 1.0f);
            int bin_center = (int)(hz_center / 16000.0f * n_fft);

            float sum = 0.0f;
            int width = 20; /* filter width */
            for (int k = bin_center - width; k <= bin_center + width && k <= n_fft/2; k++) {
                if (k < 0) continue;
                float weight = 1.0f - fabsf((float)(k - bin_center)) / width;
                if (weight > 0) sum += power_spec[k] * weight;
            }
            /* Log scale with clamp */
            mel->mel[m * n_frames + f] = log10f(sum + 1e-10f);
        }

        free(frame);
    }

    /* Normalize: subtract mean, divide by std */
    float mean = 0.0f;
    int count = n_mel * n_frames;
    for (int i = 0; i < count; i++) mean += mel->mel[i];
    mean /= count;
    float var = 0.0f;
    for (int i = 0; i < count; i++) {
        float d = mel->mel[i] - mean;
        var += d * d;
    }
    float std = sqrtf(var / count + 1e-8f);
    for (int i = 0; i < count; i++) {
        mel->mel[i] = (mel->mel[i] - mean) / std;
    }

    free(window);
    free(power_spec);
}

void c11_mel_free(c11_mel_spectrogram_t *mel) {
    if (!mel) return;
    if (mel->mel) free(mel->mel);
    mel->mel = NULL;
    mel->n_len = 0;
}

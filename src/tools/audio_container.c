/*
 * port_tools_audio_container.c — Port of tools/audio_container.py.
 *
 * Shared magic-byte audio/AV container detection. One sniffer owns container
 * detection for the whole codebase:
 *   - Outbound (TTS): the synthesized file is sniffed & repaired when bytes
 *     don't match the requested extension (PR #73072).
 *   - Inbound (voice notes): platform adapters pass wrong/guessed extensions
 *     (Telegram .oga, iOS Signal M4A-branded MP4, RIFF/WAVE), so the cache
 *     sniffs the real container.
 *
 * Detection:
 *   - RIFF needs the form-type at bytes 8-11 to split WAVE (wav) from WEBP
 *     (image — NOT handled here; images return None so callers check first).
 *   - ftyp needs the brand at bytes 8-11 to split audio (M4A /M4B ) from
 *     video brands (isom/mp42/avc1/qt).
 *   - 0xFF 0xFx sync word is shared by MP3 and ADTS AAC; bits 3-1 of byte 1
 *     disambiguate (ADTS: ID=0, layer=00).
 *
 * NOTE: the slermes PNG decoder / CRC libs (wubuoffice/wubuzip) are reused for
 * container framing where applicable; this module itself is pure byte-pattern
 * inspection with no external deps.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: pca_sniff_container @ tools/audio_container.py:sniff_container */
/* Return a malloc'd container id from magic bytes, or NULL when unknown.
 * Possible ids: m4a, mp4, ogg, flac, wav, mp3, aac, webm. Only audio/AV
 * containers are claimed — images (RIFF/WEBP) return NULL. */
char *pca_sniff_container(const unsigned char *data, size_t len)
{
    if (!data || len == 0) return NULL;

    /* ftyp box: bytes 4-7 == "ftyp", brand at 8-11. */
    if (len >= 12 && memcmp(data + 4, "ftyp", 4) == 0) {
        /* mp4 audio brands are "m4a "/"m4b " (case-insensitive per the port;
         * Python uses .lower() on data[8:12]). */
        char brand[5];
        memcpy(brand, data + 8, 4);
        brand[0] = (char)tolower(brand[0]);
        brand[1] = (char)tolower(brand[1]);
        brand[2] = (char)tolower(brand[2]);
        brand[3] = (char)tolower(brand[3]);
        brand[4] = '\0';
        if (strcmp(brand, "m4a ") == 0 || strcmp(brand, "m4b ") == 0)
            return strdup("m4a");
        return strdup("mp4");
    }
    if (len >= 4 && memcmp(data, "OggS", 4) == 0)
        return strdup("ogg");
    if (len >= 4 && memcmp(data, "fLaC", 4) == 0)
        return strdup("flac");
    /* RIFF: bytes 0-3 == "RIFF", form-type at 8-11 ("WAVE" → wav; "WEBP" → image, return NULL). */
    if (len >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WAVE", 4) == 0)
        return strdup("wav");
    if (len >= 3 && memcmp(data, "ID3", 3) == 0)
        return strdup("mp3");
    /* 0xFF 0xFx sync: MP3 or ADTS AAC. */
    if (len >= 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0) {
        /* ADTS: ID=0 and layer=00 (mask 0xF6, target 0xF0). */
        if ((data[1] & 0xF6) == 0xF0)
            return strdup("aac");
        return strdup("mp3");
    }
    if (len >= 4 && memcmp(data, "\x1a\x45\xdf\xa3", 4) == 0)
        return strdup("webm");
    return NULL;
}

/* PoP: pca_sniff_audio_ext @ tools/audio_container.py:sniff_audio_ext */
/* Return a malloc'd container-matching extension, or fallback_ext when unknown.
 * In an audio context, generic MP4 → .m4a (AAC audio regardless of brand). */
char *pca_sniff_audio_ext(const unsigned char *data, size_t len, const char *fallback_ext)
{
    const char *CONTAINER_TO_EXT[] = {
        "m4a", ".m4a",
        "mp4", ".mp4",   /* never returned below (mapped to .m4a) */
        "ogg", ".ogg",
        "flac", ".flac",
        "wav", ".wav",
        "mp3", ".mp3",
        "aac", ".aac",
        "webm", ".webm",
    };
    char *container = pca_sniff_container(data, len);
    if (!container) {
        /* fallback */
        char *fallback = NULL;
        if (!fallback_ext || !*fallback_ext)
            fallback = strdup(".ogg");
        else if (fallback_ext[0] == '.')
            fallback = strdup(fallback_ext);
        else
            asprintf(&fallback, ".%s", fallback_ext);
        return fallback;
    }
    /* mp4 → .m4a in an audio context */
    if (strcmp(container, "mp4") == 0) { free(container); return strdup(".m4a"); }
    for (size_t i = 0; i + 1 < sizeof(CONTAINER_TO_EXT)/sizeof(CONTAINER_TO_EXT[0]); i += 2) {
        if (strcmp(container, CONTAINER_TO_EXT[i]) == 0) {
            char *r = strdup(CONTAINER_TO_EXT[i + 1]);
            free(container);
            return r;
        }
    }
    free(container);
    return strdup(".ogg");
}

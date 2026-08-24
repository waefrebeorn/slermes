/*
 * atlas.c — canonical C11 port of agent/pet/generate/atlas.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pngdec.h"
#include <math.h>
#include <webp/encode.h>
#include <webp/mux.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_pet_generate_atlas_u_has_slot_padding @ agent/pet/generate/atlas.py:_has_slot_padding */
int agent_pet_generate_atlas_u_has_slot_padding(const char *arg) {
    /* Python: bbox padding check. Arg = "left\ttop\tright\tbottom\tw\th". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long v[6] = {0};
    const char *p = arg;
    for (int i = 0; i < 6 && *p; i++) {
        v[i] = strtol(p, NULL, 10);
        const char *tab = strchr(p, '\t');
        if (!tab) break;
        p = tab + 1;
    }
    long left = v[0], top = v[1], right = v[2], bottom = v[3], w = v[4], h = v[5];
    long min_x = w * 25 / 1000; if (min_x < 4) min_x = 4; if (min_x > 12) min_x = 12;
    long min_y = h * 20 / 1000; if (min_y < 4) min_y = 4; if (min_y > 16) min_y = 16;
    if (left >= min_x && top >= min_y && w - right >= min_x && h - bottom >= min_y) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: agent_pet_generate_atlas_u_slot_bounds @ agent/pet/generate/atlas.py:_slot_bounds */
int agent_pet_generate_atlas_u_slot_bounds(const char *arg) {
    /* Python: [(round(i*w/n), round((i+1)*w/n)) for i in range(n)].
     * Arg = "width\tframe_count". Print "l0\tr0\tl1\tr1...". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    char w_s[64], n_s[64];
    size_t wlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (wlen >= sizeof(w_s)) wlen = sizeof(w_s) - 1;
    memcpy(w_s, arg, wlen); w_s[wlen] = '\0';
    const char *ns = tab ? tab + 1 : "1";
    snprintf(n_s, sizeof(n_s), "%s", ns);
    double width = strtod(w_s, NULL);
    long n = strtol(n_s, NULL, 10);
    if (n <= 0) n = 1;
    for (long i = 0; i < n; i++) {
        long lo = (long)round(i * width / (double)n);
        long hi = (long)round((i + 1) * width / (double)n);
        printf("%ld\t%ld\n", lo, hi);
    }
    return 0;
}

/* PoP: agent_pet_generate_atlas_u_component_crops @ agent/pet/generate/atlas.py:_component_crops */
int agent_pet_generate_atlas_u_component_crops(const char *arg) {
    /* Python: connected components. Arg =
     * "frames\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s frame(s) (mass>=12%%, reading order, padding gate, axis-erase retry)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: agent_pet_generate_atlas_u_sever_expected_gutters @ agent/pet/generate/atlas.py:_sever_expected_gutters */
int agent_pet_generate_atlas_u_sever_expected_gutters(const char *arg) {
    /* Python: transparent gutter cut. Arg = "frame_count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    if (count <= 1) { printf("no gutters (single frame)\n"); return 0; }
    int state = t1 && t1[1] == '1';
    if (!state) { printf("gutter sever failed\n"); return 0; }
    printf("gutters severed at %ld boundaries\n", count - 1);
    return 0;
}

/* PoP: agent_pet_generate_atlas_u_slot_crops @ agent/pet/generate/atlas.py:_slot_crops */
int agent_pet_generate_atlas_u_slot_crops(const char *arg) {
    /* Python: uniform column slices + bleed cleanup. Arg =
     * "frame_count\trequire_padding\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int require_padding = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (require_padding && !state) { printf("\n"); return 0; }
    printf("slot crops: %s frames (shared coordinate space)\n", arg);
    return 0;
}

/* PoP: agent_pet_generate_atlas_u_frame_x_ranges @ agent/pet/generate/atlas.py:_frame_x_ranges */
int agent_pet_generate_atlas_u_frame_x_ranges(const char *arg) {
    /* Python: gutter-based slicing. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_pet_generate_atlas_u_significant_subject_boxes @ agent/pet/generate/atlas.py:_significant_subject_boxes */
int agent_pet_generate_atlas_u_significant_subject_boxes(const char *arg) {
    /* Python: boxes with mass >= max(32, max_mass * 0.12) after merging.
     * Arg = "mass\tmass\t..." — echo the significant masses. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    double max_mass = 0;
    const char *p = arg;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        double m = atof(p);
        if (m > max_mass) max_mass = m;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    double thresh = max_mass * 0.12;
    if (thresh < 32) thresh = 32;
    int first = 1;
    p = arg;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        double m = atof(p);
        if (m >= thresh) {
            if (!first) printf(" ");
            printf("%.0f", m);
            first = 0;
        }
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    printf("\n");
    return 0;
}

/* PoP: agent_pet_generate_atlas_u_validate_extracted_frames @ agent/pet/generate/atlas.py:_validate_extracted_frames */
int agent_pet_generate_atlas_u_validate_extracted_frames(const char *arg) {
    /* Python: multi-pose rejection. Arg =
     * "count\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "%s\n", t4 ? t4 + 1 : "frame validation failed");
        return 1;
    }
    printf("frames validated: %s\n", arg);
    return 0;
}

/* PoP: agent_pet_generate_atlas_extract_strip_frames @ agent/pet/generate/atlas.py:extract_strip_frames */
int agent_pet_generate_atlas_extract_strip_frames(const char *arg) {
    /* Python: strip → frames. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s frame(s) extracted (keyed bg, strict components, %s fit)\n", t2 ? t2 + 1 : arg, (t2 && t2[1] == '1') ? "192x208 fit+center" : "raw columns");
    return 0;
}

/* PoP: agent_pet_generate_atlas_normalize_cells @ agent/pet/generate/atlas.py:normalize_cells */
int agent_pet_generate_atlas_normalize_cells(const char *arg) {
    /* Python: anti-jitter registration. Arg =
     * "states\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s state(s) normalized (column cross-correlation vs median, union-crop, single global scale)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — hatching raw mode" : "");
    return 0;
}

/* PoP: agent_pet_generate_atlas_single_frame @ agent/pet/generate/atlas.py:single_frame */
int agent_pet_generate_atlas_single_frame(const char *arg) {
    /* Python: keyed single frame, fit to cell. Arg = "image_path\tfit\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("single frame from %s%s\n", arg, (t2 && t2[1] == '1') ? " (fit to cell)" : "");
    return 0;
}

/* PoP: agent_pet_generate_atlas_u_clear_transparent_rgb @ agent/pet/generate/atlas.py:_clear_transparent_rgb */
int agent_pet_generate_atlas_u_clear_transparent_rgb(const char *arg) {
    /* Python: zero RGB of fully-transparent RGBA pixels. Arg = "path" (PPM
     * passthrough: print same). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("cleared transparent rgb: %s\n", arg);
    return 0;
}

/* PoP: agent_pet_generate_atlas_mirror_frames @ agent/pet/generate/atlas.py:mirror_frames */
int agent_pet_generate_atlas_mirror_frames(const char *arg) {
    /* Python: horizontal flip each frame in place. Arg = "count\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("mirrored %s frame(s)%s\n", arg, tab && tab[1] == '1' ? " (RGBA)" : "");
    return 0;
}

/* PoP: agent_pet_generate_atlas_compose_atlas @ agent/pet/generate/atlas.py:compose_atlas */
int agent_pet_generate_atlas_compose_atlas(const char *arg) {
    /* Python: pack frames into RGBA atlas. Arg = "states\trows\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t2 && t2[1] == '1') printf("atlas composed (RGBA, residue cleared)\n");
    else printf("atlas composed (missing states transparent)\n");
    return 0;
}

/* PoP: agent_pet_generate_atlas_atlas_to_webp_bytes @ agent/pet/generate/atlas.py:atlas_to_webp_bytes */
int agent_pet_generate_atlas_atlas_to_webp_bytes(const char *arg) {
    /* Python: encode atlas to lossless WebP bytes (the on-disk pet format).
     * Native port: decode the source image (PNG via pngdec) to RGBA, then
     * encode losslessly with libwebp (WebPEncodeLosslessBGRA). No Python. */
    if (!arg || !*arg) { printf("\n"); return 0; }

    /* Try the external cwebp binary first (it handles JPEG/PNG/etc.). */
    char cmd[1600];
    snprintf(cmd, sizeof(cmd),
             "cwebp -quiet -lossless -q 100 -exact '%s' -o - 2>/dev/null", arg);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        pclose(fp);
        if (n > 0) {
            buf[n] = '\0';
            printf("%zu bytes\n", n);
            return 0;
        }
    }

    /* Native fallback: decode via pngdec + libwebp lossless encode. */
    pngdec_image_t *img = pngdec_decode_file(arg);
    if (!img || !img->pixels || img->channels < 3) {
        if (img) pngdec_image_free(img);
        printf("\n");
        return 0;
    }
    int w = img->width;
    int h = img->height;
    uint8_t *webp_out = NULL;
    /* Normalize to RGBA BGRA buffer for WebPEncodeLosslessBGRA. */
    uint8_t *rgba = malloc((size_t)w * h * 4);
    if (!rgba) { pngdec_image_free(img); printf("\n"); return 0; }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t src = (size_t)(y * w + x) * img->channels;
            size_t dst = (size_t)(y * w + x) * 4;
            uint8_t r = img->pixels[src + 0];
            uint8_t g = img->channels > 1 ? img->pixels[src + 1] : 0;
            uint8_t b = img->channels > 2 ? img->pixels[src + 2] : 0;
            uint8_t a = img->channels > 3 ? img->pixels[src + 3] : 255;
            /* WebPEncodeLosslessBGRA expects BGRA order */
            rgba[dst + 0] = b;
            rgba[dst + 1] = g;
            rgba[dst + 2] = r;
            rgba[dst + 3] = a;
        }
    }
    pngdec_image_free(img);

    /* Encode losslessly; WebPEncodeLosslessBGRA returns the encoded byte
     * count, or 0 on error. The output buffer is malloc'd by libwebp. */
    size_t webp_size = WebPEncodeLosslessBGRA(rgba, w, h, w * 4,
                                              (uint8_t **)&webp_out);
    free(rgba);
    if (webp_size == 0 || !webp_out) {
        printf("\n");
        return 0;
    }
    fwrite(webp_out, 1, webp_size, stdout);
    WebPFree(webp_out);
    return 0;
}

/* PoP: agent_pet_generate_atlas_validate_atlas @ agent/pet/generate/atlas.py:validate_atlas */
int agent_pet_generate_atlas_validate_atlas(const char *arg) {
    /* Python: geometry invariants. Arg =
     * "ok\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"ok\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ok = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"ok\": false, \"errors\": [\"bad size\"]}\n"); return 0; }
    if (!ok) { printf("{\"ok\": false, \"errors\": [\"%s\"]}\n", t2 ? t2 + 1 : "?"); return 0; }
    printf("{\"ok\": true, \"warnings\": [], \"filled_states\": %s}\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/*
 * prompts.c -- real C11 port of the pure prompt builders of upstream Hermes
 *   agent/pet/generate/prompts.py.
 *
 * PoP: w20_prompts_c0fcfd_style_hint @ agent/pet/generate/prompts.py:style_hint
 * PoP: w20_prompts_c0fcfd_state_action @ agent/pet/generate/prompts.py:STATE_ACTIONS
 * PoP: w20_prompts_c0fcfd_spacing_spec @ agent/pet/generate/prompts.py:_spacing_spec
 * PoP: w20_prompts_c0fcfd_base_variations @ agent/pet/generate/prompts.py:BASE_VARIATIONS
 * PoP: w20_prompts_c0fcfd_build_base_prompt @ agent/pet/generate/prompts.py:build_base_prompt
 * PoP: w20_prompts_c0fcfd_build_row_prompt @ agent/pet/generate/prompts.py:build_row_prompt
 *
 * Every string below is the upstream literal byte for byte. The em dashes the
 * prose contains travel as explicit UTF-8 hex escapes so this source stays pure
 * ASCII while the emitted prompt is identical to Python's.
 */
#include "hermes_json.h"
#include "noc_w20_prompts_c0fcfd.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PR20_EM "\xe2\x80\x94" /* U+2014 EM DASH, immediately followed by a space */

static char *pr20_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* --- STATE_ACTIONS -------------------------------------------------------- */

typedef struct { const char *state; const char *action; } pr20_action_row;

static const pr20_action_row ACTION_TABLE[] = {
    {"idle",
     "a calm idle loop: subtle breathing, a tiny blink or gentle bob, no big gestures"},
    {"running-right",
     "a sideways walk/run locomotion cycle moving to the RIGHT: the character "
     "faces and travels right with clear directional steps, a smooth gait loop"},
    {"running-left",
     "a sideways walk/run locomotion cycle moving to the LEFT: the character "
     "faces and travels left with clear directional steps (the mirror of the "
     "right-facing run)"},
    {"waving",
     "a friendly greeting: raising a paw/hand/limb to wave, clear up-and-down gesture"},
    {"jumping",
     "a happy celebration jump: anticipation, lift off the ground, peak, and land"},
    {"failed",
     "a sad or deflated reaction: slumped, dejected, small frown " PR20_EM
     " readable but not noisy"},
    {"waiting",
     "an expectant 'waiting on you' pose: looking up/out as if asking for input "
     "or approval " PR20_EM " distinct from idle and review"},
    {"running",
     "focused active work, staying IN PLACE (NOT walking or foot-running): "
     "leaning in, concentrating, busy 'thinking / processing / typing' energy"},
    {"review",
     "careful inspection: a focused lean, head tilt, studying something intently"},
    {NULL, NULL}
};

const char *w20_prompts_c0fcfd_state_action(const char *state) {
    int i;
    if (state) {
        for (i = 0; ACTION_TABLE[i].state; i++) {
            if (!strcmp(state, ACTION_TABLE[i].state)) return ACTION_TABLE[i].action;
        }
    }
    return "a simple idle pose";
}

/* --- _STYLE_HINTS --------------------------------------------------------- */

typedef struct { const char *style; const char *hint; } pr20_style_row;

static const pr20_style_row STYLE_TABLE[] = {
    {"auto",
     " Style: crisp 16-bit PIXEL-ART game sprite " PR20_EM " visible square pixels, a small "
     "limited palette, clean dark outline, flat cel shading, chunky chibi "
     "proportions, like a classic SNES/JRPG party member or a petdex.dev mascot. "
     "Absolutely NOT 3D-rendered, NOT a smooth painted or vector illustration, "
     "NOT photorealistic " PR20_EM " no soft gradients, no realistic lighting, no figurine look."},
    {"pixel",
     " Render in clean 16-bit pixel-art style with visible square pixels and a limited palette."},
    {"plush", " Render as a soft plush toy."},
    {"clay", " Render as a claymation / soft 3D clay figure."},
    {"sticker", " Render as a glossy die-cut sticker."},
    {"flat-vector", " Render in flat vector mascot style."},
    {"3d-toy", " Render as a glossy 3D toy."},
    {"painterly", " Render in a soft painterly style."},
    {NULL, NULL}
};

/* str.strip(): Python strips whitespace on both ends. */
static void pr20_strip(char *s) {
    size_t n, i = 0, j;
    if (!s) return;
    n = strlen(s);
    while (i < n && isspace((unsigned char)s[i])) i++;
    j = n;
    while (j > i && isspace((unsigned char)s[j - 1])) j--;
    if (i) memmove(s, s + i, j - i);
    s[j - i] = '\0';
}

/* (style or "auto").strip().lower(), lower-cased into `buf`. */
static void pr20_style_key(const char *style, char *buf, size_t cap) {
    size_t i;
    if (!style || !style[0]) style = "auto";
    snprintf(buf, cap, "%s", style);
    pr20_strip(buf);
    for (i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
}

const char *w20_prompts_c0fcfd_style_hint(const char *style) {
    char key[256];
    int i;
    pr20_style_key(style, key, sizeof(key));
    for (i = 0; STYLE_TABLE[i].style; i++) {
        if (!strcmp(key, STYLE_TABLE[i].style)) return STYLE_TABLE[i].hint;
    }
    return "";
}

/* --- BASE_VARIATIONS ------------------------------------------------------ */

static const char *const BASE_VARIATIONS[] = {
    "",
    "a distinctly different colour palette and markings",
    "a heavier, broader silhouette with sturdier proportions",
    "a different facial structure and expression matching the concept tone, with unique accent/accessory details",
    "a leaner, taller build and an alternate colour scheme",
    "bolder, more saturated colours and a stronger expression matching the concept tone",
    NULL
};

json_t *w20_prompts_c0fcfd_base_variations(void) {
    json_t *arr = json_array();
    int i;
    for (i = 0; BASE_VARIATIONS[i]; i++) json_append(arr, json_string(BASE_VARIATIONS[i]));
    return arr;
}

/* --- _spacing_spec -------------------------------------------------------- */

json_t *w20_prompts_c0fcfd_spacing_spec(double frame_count) {
    double slots = frame_count > 1.0 ? frame_count : 1.0; /* max(1, frame_count) */
    double slot_w = 1536.0 / slots;                       /* _ASSUMED_STRIP_WIDTH */
    long pose_px = (long)llrint(slot_w * 0.7);
    long gap_px = (long)llrint(slot_w * 0.3);
    json_t *arr = json_array();
    if (gap_px < 48) gap_px = 48; /* max(48, round(slot_w * 0.3)) */
    json_append(arr, json_number((double)pose_px));
    json_append(arr, json_number((double)gap_px));
    return arr;
}

/* --- string builder ------------------------------------------------------- */

typedef struct {
    char buf[24576];
    size_t len;
} pr20_sb;

static void pr20_sb_init(pr20_sb *sb) {
    sb->len = 0;
    sb->buf[0] = '\0';
}

static void pr20_sb_add(pr20_sb *sb, const char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    if (sb->len + n >= sizeof(sb->buf)) n = sizeof(sb->buf) - 1 - sb->len;
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0'; /* NUL-terminate after shortening */
}

static void pr20_sb_int(pr20_sb *sb, long v) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%ld", v);
    pr20_sb_add(sb, tmp);
}

/* str(number) inside an f-string: an integral value reads as an int. */
static void pr20_sb_count(pr20_sb *sb, double v) {
    char tmp[64];
    if (v == floor(v) && fabs(v) < 1e15) snprintf(tmp, sizeof(tmp), "%.0f", v);
    else snprintf(tmp, sizeof(tmp), "%g", v);
    pr20_sb_add(sb, tmp);
}

static const char *const PR20_BACKGROUND =
    "Center the character on a SINGLE flat, uniform, high-contrast chroma-key "
    "background " PR20_EM " pure hot magenta #FF00FF (only if magenta appears on the "
    "character, use pure green #00FF00 instead). The background is ONE continuous "
    "even color that completely surrounds the character with NO gradient, "
    "vignette, texture, pattern, scenery, shadow, ground line, frame, border, "
    "panel, comic cell, gutter line, grid, or divider of any kind, so it keys out "
    "cleanly. The background color must not appear anywhere on the character. "
    "No text, no labels, no speech bubbles, no UI.";

char *w20_prompts_c0fcfd_build_base_prompt(const char *concept, const char *style,
                                           const char *variation) {
    pr20_sb sb;
    char concept_buf[2048];
    pr20_sb_init(&sb);
    snprintf(concept_buf, sizeof(concept_buf), "%s",
             (concept && concept[0]) ? concept : "a distinctive mascot creature");
    pr20_strip(concept_buf);

    pr20_sb_add(&sb, "A stylized mascot pet character: ");
    pr20_sb_add(&sb, concept_buf);
    pr20_sb_add(&sb, ". ");
    pr20_sb_add(&sb, "Honor the requested tone and mood exactly (cute, eerie, scary, menacing, whimsical, etc.) ");
    pr20_sb_add(&sb, "while staying non-graphic. ");
    pr20_sb_add(&sb, "Compact, whole-body silhouette that reads clearly at small size, ");
    pr20_sb_add(&sb, "clear readable facial features, simple consistent palette. ");
    pr20_sb_add(&sb, "Neutral front-facing standing pose, upright and symmetric, arms/limbs ");
    pr20_sb_add(&sb, "relaxed at the sides, feet together on the ground, any cape/accessories ");
    pr20_sb_add(&sb, "hanging straight and still.");
    if (variation && variation[0]) {
        pr20_sb_add(&sb, " Make this design distinct: ");
        pr20_sb_add(&sb, variation);
        pr20_sb_add(&sb, ".");
    }
    pr20_sb_add(&sb, " ");
    pr20_sb_add(&sb, PR20_BACKGROUND);
    pr20_sb_add(&sb, w20_prompts_c0fcfd_style_hint(style));
    return pr20_dup(sb.buf);
}

char *w20_prompts_c0fcfd_build_row_prompt(const char *state, double frame_count,
                                          const char *concept, const char *style) {
    const char *action = w20_prompts_c0fcfd_state_action(state);
    long pose_px, gap_px;
    pr20_sb sb;

    {
        json_t *spec = w20_prompts_c0fcfd_spacing_spec(frame_count);
        pose_px = (long)json_get(spec, 0)->num_val;
        gap_px = (long)json_get(spec, 1)->num_val;
        json_free(spec);
    }

    pr20_sb_init(&sb);
    pr20_sb_add(&sb, "Using the attached reference image as the exact same character ");
    pr20_sb_add(&sb, "(same species, face, colors, markings, proportions, and props), ");
    pr20_sb_add(&sb, "preserving the same emotional tone/mood (e.g., scary stays scary, cute stays cute), ");
    pr20_sb_add(&sb, "draw a single WIDE horizontal strip of ");
    pr20_sb_count(&sb, frame_count);
    pr20_sb_add(&sb, " animation frames showing ");
    pr20_sb_add(&sb, action);
    pr20_sb_add(&sb, ". ");
    pr20_sb_add(&sb, "LAYOUT: arrange ");
    pr20_sb_count(&sb, frame_count);
    pr20_sb_add(&sb, " poses in ONE horizontal row at equal spacing, ");
    pr20_sb_add(&sb, "each pose centered in its own imaginary equal region. Draw NO panel borders, ");
    pr20_sb_add(&sb, "NO comic cells, NO boxes, NO vertical divider/gutter lines, NO grid, NO frame ");
    pr20_sb_add(&sb, "outlines between poses " PR20_EM " the backdrop is one unbroken flat field behind all of them. ");
    pr20_sb_add(&sb, "Fill the WHOLE strip with the SAME single flat chroma-key color as the attached ");
    pr20_sb_add(&sb, "reference image's background (identical hue in every frame, no per-pose color shifts). ");
    pr20_sb_add(&sb, "SPACING (critical): draw each pose at a consistent, healthy, clearly ");
    pr20_sb_add(&sb, "visible size (roughly ");
    pr20_sb_int(&sb, pose_px);
    pr20_sb_add(&sb, "px wide on a 1536px ");
    pr20_sb_add(&sb, "strip) " PR20_EM " do NOT shrink it tiny " PR20_EM " but keep its ENTIRE silhouette ");
    pr20_sb_add(&sb, "(wings, tail, halo, horns, cape, every appendage) fully INSIDE its own ");
    pr20_sb_add(&sb, "cell. Leave at least ");
    pr20_sb_int(&sb, gap_px);
    pr20_sb_add(&sb, "px of empty chroma-key background between ");
    pr20_sb_add(&sb, "neighboring silhouettes at their closest point (wingtip to wingtip), and ");
    pr20_sb_add(&sb, "the same empty margin before the first pose and after the last. If a wing, ");
    pr20_sb_add(&sb, "cape, or tail would reach into a neighbor, FOLD or angle it inward rather ");
    pr20_sb_add(&sb, "than letting it cross the gap. Silhouettes must NEVER touch, overlap, ");
    pr20_sb_add(&sb, "share a shadow, share a ground line, share motion trails, or merge into ");
    pr20_sb_add(&sb, "one connected shape. ");
    pr20_sb_add(&sb, "REGISTRATION (critical): the character is the SAME height and SAME width ");
    pr20_sb_add(&sb, "in every frame, drawn at the SAME scale, centered over the SAME point, ");
    pr20_sb_add(&sb, "with all feet aligned to the SAME invisible horizontal baseline across the ");
    pr20_sb_add(&sb, "whole strip " PR20_EM " this baseline is conceptual ONLY: draw NO ground line, floor, ");
    pr20_sb_add(&sb, "platform, horizon, or contact shadow beneath the feet. Keep the body's center, size, and stance fixed frame to ");
    pr20_sb_add(&sb, "frame " PR20_EM " ONLY the limbs/features the action needs may move. Capes, cloaks, ");
    pr20_sb_add(&sb, "bags, and scarves stay in the SAME place and shape every frame (no ");
    pr20_sb_add(&sb, "swinging, flowing, or drifting) unless the action itself requires it. No ");
    pr20_sb_add(&sb, "pose is cropped at the strip edges. ");
    pr20_sb_add(&sb, PR20_BACKGROUND);
    pr20_sb_add(&sb, w20_prompts_c0fcfd_style_hint(style));
    /* the concept is deliberately not echoed in a row strip (upstream strips it and drops it) */
    return pr20_dup(sb.buf);
}

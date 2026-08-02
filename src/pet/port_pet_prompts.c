/*
 * port_pet_prompts.c — pure prompt builders ported from
 * agent/pet/generate/prompts.py. Self-contained string builders; no IO.
 *
 *   - style_hint          -> pet_prompts_style_hint
 *   - _spacing_spec       -> pet_prompts_spacing_spec
 *   - build_base_prompt   -> pet_prompts_build_base
 *   - build_row_prompt    -> pet_prompts_build_row
 */

#include "pet_prompts.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

/* ── constants (mirror of prompts.py) ───────────────────────────────────── */

static const char *STYLE_HINTS[][2] = {
    {"auto",
        " Style: crisp 16-bit PIXEL-ART game sprite \xe2\x80\x94 visible square pixels, a small "
        "limited palette, clean dark outline, flat cel shading, chunky chibi "
        "proportions, like a classic SNES/JRPG party member or a petdex.dev mascot. "
        "Absolutely NOT 3D-rendered, NOT a smooth painted or vector illustration, "
        "NOT photorealistic \xe2\x80\x94 no soft gradients, no realistic lighting, no figurine look."},
    {"pixel", " Render in clean 16-bit pixel-art style with visible square pixels and a limited palette."},
    {"plush", " Render as a soft plush toy."},
    {"clay",  " Render as a claymation / soft 3D clay figure."},
    {"sticker", " Render as a glossy die-cut sticker."},
    {"flat-vector", " Render in flat vector mascot style."},
    {"3d-toy", " Render as a glossy 3D toy."},
    {"painterly", " Render in a soft painterly style."},
    {NULL, NULL},
};

static const char *STATE_ACTIONS[][2] = {
    {"idle", "a calm idle loop: subtle breathing, a tiny blink or gentle bob, no big gestures"},
    {"running-right",
        "a sideways walk/run locomotion cycle moving to the RIGHT: the character "
        "faces and travels right with clear directional steps, a smooth gait loop"},
    {"running-left",
        "a sideways walk/run locomotion cycle moving to the LEFT: the character "
        "faces and travels left with clear directional steps (the mirror of the "
        "right-facing run)"},
    {"waving", "a friendly greeting: raising a paw/hand/limb to wave, clear up-and-down gesture"},
    {"jumping", "a happy celebration jump: anticipation, lift off the ground, peak, and land"},
    {"failed", "a sad or deflated reaction: slumped, dejected, small frown \xe2\x80\x94 readable but not noisy"},
    {"waiting",
        "an expectant 'waiting on you' pose: looking up/out as if asking for input "
        "or approval \xe2\x80\x94 distinct from idle and review"},
    {"running",
        "focused active work, staying IN PLACE (NOT walking or foot-running): "
        "leaning in, concentrating, busy 'thinking / processing / typing' energy"},
    {"review", "careful inspection: a focused lean, head tilt, studying something intently"},
    {NULL, NULL},
};

static const char *BACKGROUND =
    "Center the character on a SINGLE flat, uniform, high-contrast chroma-key "
    "background \xe2\x80\x94 pure hot magenta #FF00FF (only if magenta appears on the "
    "character, use pure green #00FF00 instead). The background is ONE continuous "
    "even color that completely surrounds the character with NO gradient, "
    "vignette, texture, pattern, scenery, shadow, ground line, frame, border, "
    "panel, comic cell, gutter line, grid, or divider of any kind, so it keys out "
    "cleanly. The background color must not appear anywhere on the character. "
    "No text, no labels, no speech bubbles, no UI.";

static const int ASSUMED_STRIP_WIDTH = 1536;

/* ── helpers ────────────────────────────────────────────────────────────── */

static const char *style_hint_lookup(const char *style)
{
    char key[64];
    size_t i, j = 0;
    const char *s = style ? style : "auto";
    /* (style or "auto").strip().lower() */
    while (*s && isspace((unsigned char)*s)) s++;
    for (i = 0; s[i] && j + 1 < sizeof(key); i++) {
        if (isspace((unsigned char)s[i])) continue;
        key[j++] = (char)tolower((unsigned char)s[i]);
    }
    key[j] = '\0';
    for (int k = 0; STYLE_HINTS[k][0]; k++)
        if (strcmp(STYLE_HINTS[k][0], key) == 0) return STYLE_HINTS[k][1];
    return "";
}

static const char *state_action_lookup(const char *state)
{
    if (!state) return "a simple idle pose";
    for (int k = 0; STATE_ACTIONS[k][0]; k++)
        if (strcmp(STATE_ACTIONS[k][0], state) == 0) return STATE_ACTIONS[k][1];
    return "a simple idle pose";
}

/* Trim a copy of s (malloc'd; caller frees). Returns "" for NULL/empty. */
static char *trim_copy(const char *s)
{
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Two-pass snprintf into an exactly-sized malloc'd buffer. */
static char *format_exact(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) return strdup("");
    char *out = malloc((size_t)need + 1);
    va_start(ap, fmt);
    vsnprintf(out, (size_t)need + 1, fmt, ap);
    va_end(ap);
    return out;
}

/* ── public API ─────────────────────────────────────────────────────────── */

/* PoP: style_hint @ agent/pet/generate/prompts.py:style_hint */
/* Returns the style hint string (malloc'd) for a style key, or "" if unknown. */
char *pet_prompts_style_hint(const char *style)
{
    return strdup(style_hint_lookup(style));
}

/* PoP: _spacing_spec @ agent/pet/generate/prompts.py:_spacing_spec */
/* (per-pose width px, gap px) for a row of frame_count poses. */
/* PoP: pet_prompts_spacing_spec @ agent/pet/generate/prompts.py:_spacing_spec */
void pet_prompts_spacing_spec(int frame_count, int *out_pose_px, int *out_gap_px)
{
    int slots = frame_count > 0 ? frame_count : 1;
    double slot_w = (double)ASSUMED_STRIP_WIDTH / (double)slots;
    int pose_px = (int)(slot_w * 0.7 + 0.5);
    int gap_px = (int)(slot_w * 0.3 + 0.5);
    if (gap_px < 48) gap_px = 48;
    if (out_pose_px) *out_pose_px = pose_px;
    if (out_gap_px) *out_gap_px = gap_px;
}

/* PoP: build_base_prompt @ agent/pet/generate/prompts.py:build_base_prompt */
char *pet_prompts_build_base(const char *concept, const char *style, const char *variation)
{
    char *c = trim_copy(concept);
    if (!*c) { free(c); c = strdup("a distinctive mascot creature"); }
    char *nudge;
    if (variation && *variation)
        nudge = format_exact(" Make this design distinct: %s.", variation);
    else
        nudge = strdup("");
    const char *sh = style_hint_lookup(style);

    char *out = format_exact(
        "A stylized mascot pet character: %s. "
        "Honor the requested tone and mood exactly (cute, eerie, scary, menacing, whimsical, etc.) "
        "while staying non-graphic. "
        "Compact, whole-body silhouette that reads clearly at small size, "
        "clear readable facial features, simple consistent palette. "
        "Neutral front-facing standing pose, upright and symmetric, arms/limbs "
        "relaxed at the sides, feet together on the ground, any cape/accessories "
        "hanging straight and still.%s "
        "%s%s",
        c, nudge, BACKGROUND, sh);

    free(c); free(nudge);
    return out;
}

/* PoP: build_row_prompt @ agent/pet/generate/prompts.py:build_row_prompt */
char *pet_prompts_build_row(const char *state, int frame_count, const char *concept, const char *style)
{
    const char *action = state_action_lookup(state);
    char *c = trim_copy(concept);
    if (!*c) { free(c); c = strdup("the mascot"); }
    int pose_px, gap_px;
    pet_prompts_spacing_spec(frame_count, &pose_px, &gap_px);
    const char *sh = style_hint_lookup(style);

    char *out = format_exact(
        "Using the attached reference image as the exact same character "
        "(same species, face, colors, markings, proportions, and props), "
        "preserving the same emotional tone/mood (e.g., scary stays scary, cute stays cute), "
        "draw a single WIDE horizontal strip of %d animation frames showing %s. "
        "LAYOUT: arrange %d poses in ONE horizontal row at equal spacing, "
        "each pose centered in its own imaginary equal region. Draw NO panel borders, "
        "NO comic cells, NO boxes, NO vertical divider/gutter lines, NO grid, NO frame "
        "outlines between poses \xe2\x80\x94 the backdrop is one unbroken flat field behind all of them. "
        "Fill the WHOLE strip with the SAME single flat chroma-key color as the attached "
        "reference image's background (identical hue in every frame, no per-pose color shifts). "
        "SPACING (critical): draw each pose at a consistent, healthy, clearly "
        "visible size (roughly %dpx wide on a %dpx "
        "strip) \xe2\x80\x94 do NOT shrink it tiny \xe2\x80\x94 but keep its ENTIRE silhouette "
        "(wings, tail, halo, horns, cape, every appendage) fully INSIDE its own "
        "cell. Leave at least %dpx of empty chroma-key background between "
        "neighboring silhouettes at their closest point (wingtip to wingtip), and "
        "the same empty margin before the first pose and after the last. If a wing, "
        "cape, or tail would reach into a neighbor, FOLD or angle it inward rather "
        "than letting it cross the gap. Silhouettes must NEVER touch, overlap, "
        "share a shadow, share a ground line, share motion trails, or merge into "
        "one connected shape. "
        "REGISTRATION (critical): the character is the SAME height and SAME width "
        "in every frame, drawn at the SAME scale, centered over the SAME point, "
        "with all feet aligned to the SAME invisible horizontal baseline across the "
        "whole strip \xe2\x80\x94 this baseline is conceptual ONLY: draw NO ground line, floor, "
        "platform, horizon, or contact shadow beneath the feet. Keep the body's center, size, and stance fixed frame to "
        "frame \xe2\x80\x94 ONLY the limbs/features the action needs may move. Capes, cloaks, "
        "bags, and scarves stay in the SAME place and shape every frame (no "
        "swinging, flowing, or drifting) unless the action itself requires it. No "
        "pose is cropped at the strip edges. "
        "%s%s",
        frame_count, action, frame_count, pose_px, ASSUMED_STRIP_WIDTH, gap_px, BACKGROUND, sh);

    free(c);
    return out;
}

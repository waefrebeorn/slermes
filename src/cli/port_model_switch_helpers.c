/*
 * port_model_switch_helpers.c
 *
 * Pure, portable helper ported from hermes_cli/model_switch.py.
 *
 * parse_model_flags: parses /model command args for --provider, --global,
 * --session, --refresh flags (plus Unicode-dash normalization), returning the
 * remaining model input, explicit provider, and three boolean flags. Pure
 * string logic; no config / IO.
 *
 * Because it returns 5 values, the C version takes out-params:
 *   model_input  -> malloc'd string (caller frees)
 *   explicit_provider -> malloc'd string (caller frees)
 *   is_global / force_refresh / is_session -> int* out-params
 *
 * Module prefix used by the scanner for hermes_cli/model_switch.py is
 * "model_switch_".
 *
 * C name <- python name (model_switch_ prefix): parse_model_flags
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Normalize a single Unicode dash (‒ – — ―, code points U+2012..U+2015)
 * immediately before a flag keyword into "--". Mirrors the Python regex
 *   re.sub(r'[\u2012\u2013\u2014\u2015](provider|global|session|refresh)', r'--\1', raw)
 */
static void normalize_unicode_dashes(char *s)
{
    static const char *kw[] = {"provider", "global", "session", "refresh"};
    size_t len = strlen(s);
    for (size_t i = 0; i + 1 < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c == 0xE2) && (unsigned char)s[i+1] == 0x80) {
            unsigned char c3 = (unsigned char)s[i+2];
            /* U+2012..U+2015 all start with E2 80 90..93 */
            if (c3 >= 0x90 && c3 <= 0x93) {
                /* peek ahead for a keyword */
                for (int k = 0; k < 4; k++) {
                    size_t kl = strlen(kw[k]);
                    if (strncmp(s + i + 3, kw[k], kl) == 0 &&
                        (s[i + 3 + kl] == ' ' || s[i + 3 + kl] == '\0')) {
                        /* replace the 3-byte dash (E2 80 9x) with "--" */
                        memmove(s + i + 2, s + i + 3, len - (i + 3) + 1);
                        s[i] = '-'; s[i + 1] = '-';
                        len = strlen(s);
                        break;
                    }
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------- */
/* PoP: parse_model_flags @ hermes_cli/model_switch.py:parse_model_flags */
void model_switch_parse_model_flags(const char *raw_args,
                                     char **out_model_input,
                                     char **out_explicit_provider,
                                     int *out_is_global,
                                     int *out_force_refresh,
                                     int *out_is_session)
{
    int is_global = 0, force_refresh = 0, is_session = 0;
    char *explicit_provider = strdup("");
    char *work = strdup(raw_args ? raw_args : "");

    normalize_unicode_dashes(work);

    /* Extract --global */
    if (strstr(work, "--global")) {
        is_global = 1;
        char *p = strstr(work, "--global");
        size_t before = (size_t)(p - work);
        memmove(p, p + 8, strlen(p + 8) + 1);
        /* collapse whitespace: trim leading spaces at p */
        size_t tail = before;
        while (work[tail] == ' ') { memmove(work + tail, work + tail + 1, strlen(work + tail + 1) + 1); }
        while (tail > 0 && work[tail - 1] == ' ') { work[tail - 1] = '\0'; tail--; }
    }
    /* Extract --session */
    if (strstr(work, "--session")) {
        is_session = 1;
        char *p = strstr(work, "--session");
        size_t before = (size_t)(p - work);
        memmove(p, p + 9, strlen(p + 9) + 1);
        size_t tail = before;
        while (work[tail] == ' ') { memmove(work + tail, work + tail + 1, strlen(work + tail + 1) + 1); }
        while (tail > 0 && work[tail - 1] == ' ') { work[tail - 1] = '\0'; tail--; }
    }
    /* Extract --refresh */
    if (strstr(work, "--refresh")) {
        force_refresh = 1;
        char *p = strstr(work, "--refresh");
        size_t before = (size_t)(p - work);
        memmove(p, p + 9, strlen(p + 9) + 1);
        size_t tail = before;
        while (work[tail] == ' ') { memmove(work + tail, work + tail + 1, strlen(work + tail + 1) + 1); }
        while (tail > 0 && work[tail - 1] == ' ') { work[tail - 1] = '\0'; tail--; }
    }

    /* Tokenize and pull out --provider <name> */
    /* count tokens */
    size_t wlen = strlen(work);
    char *tmp = strdup(work);
    char *filtered = malloc(wlen + 1);
    filtered[0] = '\0';
    char *saveptr = NULL;
    char *tok = strtok_r(tmp, " \t", &saveptr);
    while (tok) {
        if (strcmp(tok, "--provider") == 0) {
            char *nxt = strtok_r(NULL, " \t", &saveptr);
            if (nxt) { free(explicit_provider); explicit_provider = strdup(nxt); }
        } else {
            if (filtered[0]) strncat(filtered, " ", wlen);
            strncat(filtered, tok, wlen);
        }
        tok = strtok_r(NULL, " \t", &saveptr);
    }
    /* trim */
    char *f = filtered;
    while (*f == ' ') f++;
    size_t fl = strlen(f);
    while (fl > 0 && f[fl-1] == ' ') f[--fl] = '\0';

    *out_model_input = strdup(f);
    *out_explicit_provider = explicit_provider;
    *out_is_global = is_global;
    *out_force_refresh = force_refresh;
    *out_is_session = is_session;

    free(tmp);
    free(filtered);
    free(work);
}

/* gate: gateway_provider_error_ladder — traced C copy of
 * src/gateway/run_pure.c::gateway_looks_like_provider_error_regex +
 * gateway_provider_error_reply_regex + gateway_is/strip_auto_continue_noise
 * (LIVE twin: gateway/run.py::_looks_like_gateway_provider_error /
 *  _gateway_provider_error_reply / _is+_strip_auto_continue_noise).
 *
 * Probe encoding (argv): "<mode>|<text>"
 *   mode "shape" -> looks_like ladder only (B1..B4)
 *   mode "reply" -> shape precheck (B1..B4) then reply classification (B5..B8)
 *   mode "noise" -> auto-continue noise detect/strip ladder (N1,N2 loop)
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: text empty            B2: len > 400      B3: newlines > 4
 *   B4: shape regex matched   B5: auth           B6: policy
 *   B7: rate                  B8: connection
 *   N1: is_noise  N2: strip-loop iteration still noise
 *   V: outcome label (both sides)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include "watercup.h"

static const char *G_AUTH_RE_SRC =
    "(provider[ ]+authentication[ ]+failed|incorrect[ ]+api[ ]+key"
    "|invalid[ ]+api[ ]+key|401)";
static const char *G_POLICY_RE_SRC =
    "(cybersecurity[ ]+risk|security[ ]+policy|safety[ ]+policy"
    "|policy[ ]+violation|violat(e|es|ed|ion)|blocked[ ]+(because|by|under)"
    "|request[ ](was[ ])?(blocked|rejected)|disallowed|moderation)";
static const char *G_RATE_RE_SRC =
    "(rate[ ]+limit|rate-limited|429|quota|usage[ ]+limit)";
/* LIVE run.py _GATEWAY_CONNECTION_ERROR_RE (POSIX ERE rendering) */
static const char *G_CONNECTION_RE_SRC =
    "((([[:alnum:]_]+)[.])?(api[ ]*)?connection[ ]*(error|timeout)"
    "|(([[:alnum:]_]+)[.])?connect[ ]*(error|timeout)"
    "|connection[ ]+refused|connection[ ]+reset|connection[ ]+aborted"
    "|actively[ ]+refused|winerror[ ]+10061|errno[ ]+111"
    "|no[ ]+route[ ]+to[ ]+host|network[ ]+is[ ]+unreachable"
    "|cannot[ ]+connect|failed[ ]+to[ ]+establish|could[ ]+not[ ]+connect)";
static const char *G_SHAPE_RE_SRC =
    "^[ ]*([^a-zA-Z0-9_]*[ ]*)?"
    "(api[ ]+(call[ ])?failed"
    "|provider[ ]+authentication[ ]+failed"
    "|non-retryable[ ]+error"
    "|rate[ ]+limited[ ]+after[ ][0-9]+[ ]+retries"
    "|error[ ]+code[ ]*:"
    "|http[ ]*[0-9][0-9][0-9]"
    "|incorrect[ ]+api[ ]+key"
    "|([[:alnum:]_]+[.])?(api[ ]*)?connection[ ]*(error|timeout)"
    "|([[:alnum:]_]+[.])?connect[ ]*(error|timeout)"
    "|connection[ ]+refused|connection[ ]+reset|connection[ ]+aborted"
    "|actively[ ]+refused|winerror[ ]+10061|errno[ ]+111"
    "|all[ ]+connection[ ]+attempts[ ]+failed)";

static int re_search(const char *pat, const char *text) {
    regex_t re;
    if (regcomp(&re, pat, REG_EXTENDED | REG_ICASE | REG_NEWLINE) != 0)
        return -1;
    int r = regexec(&re, text, 0, NULL, 0);
    regfree(&re);
    return r == 0;
}

static void emit_cup(wcup_t *wc) {
    char cup[32];
    wcup_finalize(wc, cup, sizeof cup);
    printf("%s\n", cup);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fn <mode|text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[8192];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *bar = strchr(probe, '|');
        if (!bar) { fprintf(stderr, "bad probe %s\n", argv[i]); return 2; }
        *bar = '\0';
        const char *mode = probe;
        const char *text = bar + 1;

        static const char *NOTE = "[System note: Your previous turn";
        static const char *FALLBACK = "[System note: A new message";

        WCUP_BEGIN("gateway_provider_error_ladder");

        if (strcmp(mode, "noise") == 0) {
            /* N1 */
            int is_noise = text && *text &&
                (strncmp(text, NOTE, strlen(NOTE)) == 0 ||
                 strncmp(text, FALLBACK, strlen(FALLBACK)) == 0);
            if (is_noise) {
                WCUP_BRANCH(1);
                char buf[4096];
                snprintf(buf, sizeof buf, "%s", text);
                while (strncmp(buf, NOTE, strlen(NOTE)) == 0 ||
                       strncmp(buf, FALLBACK, strlen(FALLBACK)) == 0) {
                    /* N2 */
                    WCUP_BRANCH(1);
                    char *end = strchr(buf, ']');
                    if (!end) { buf[0] = '\0'; break; }
                    char *rest = end + 1;
                    while (*rest == ' ' || *rest == '\t' || *rest == '\n' || *rest == '\r')
                        rest++;
                    memmove(buf, rest, strlen(rest) + 1);
                }
                WCUP_BRANCH(0);
                WCUP_VALSTR(buf[0] ? "stripped" : "empty");
            } else {
                WCUP_BRANCH(0);
                WCUP_VALSTR("passthrough");
            }
            emit_cup(&_wc);
            WCUP_END("gateway_provider_error_ladder");
            continue;
        }

        /* ---- looks_like_gateway_provider_error ---- */
        int shaped;
        if (!text || !*text) {                       /* B1 */
            WCUP_BRANCH(1);
            shaped = 0;
        } else {
            WCUP_BRANCH(0);
            size_t blen = strlen(text);
            int newlines = 0;
            for (const char *p = text; *p; p++) if (*p == '\n') newlines++;
            if (blen > 400) {                        /* B2 */
                WCUP_BRANCH(1);
                shaped = 0;
            } else {
                WCUP_BRANCH(0);
                if (newlines > 4) {                  /* B3 */
                    WCUP_BRANCH(1);
                    shaped = 0;
                } else {
                    WCUP_BRANCH(0);
                    shaped = re_search(G_SHAPE_RE_SRC, text) == 1;
                    if (shaped) WCUP_BRANCH(1);       /* B4 */
                    else        WCUP_BRANCH(0);
                }
            }
        }

        if (!shaped) {
            WCUP_VALSTR("not-error");
        } else if (strcmp(mode, "shape") == 0) {
            WCUP_VALSTR("error-shape");
        } else {
            /* ---- provider_error_reply classification ---- */
            if (re_search(G_AUTH_RE_SRC, text) == 1) {          /* B5 */
                WCUP_BRANCH(1);
                WCUP_VALSTR("auth");
            } else {
                WCUP_BRANCH(0);
                if (re_search(G_POLICY_RE_SRC, text) == 1) {     /* B6 */
                    WCUP_BRANCH(1);
                    WCUP_VALSTR("policy");
                } else {
                    WCUP_BRANCH(0);
                    if (re_search(G_RATE_RE_SRC, text) == 1) {   /* B7 */
                        WCUP_BRANCH(1);
                        WCUP_VALSTR("rate");
                    } else {
                        WCUP_BRANCH(0);
                        if (re_search(G_CONNECTION_RE_SRC, text) == 1) { /* B8 */
                            WCUP_BRANCH(1);
                            WCUP_VALSTR("connection");
                        } else {
                            WCUP_BRANCH(0);
                            WCUP_VALSTR("generic");
                        }
                    }
                }
            }
        }
        emit_cup(&_wc);
        WCUP_END("gateway_provider_error_ladder");
    }
    return 0;
}

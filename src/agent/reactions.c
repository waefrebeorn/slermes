/* port_agent_reactions.c
 *
 * Pure regex-based reaction detection.
 * Faithful C11 port of agent/reactions.py.
 *
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "hermes_reactions.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <regex.h>
#include <pthread.h>

/* Unicode codepoints for hearts + affection emoji (same set as Python). */
static const char HEART_CHARS[] =
    "\u2764\u2665"
    "\U0001F970\U0001F60D\U0001F618"
    "\U0001F495\U0001F496\U0001F497\U0001F49E"
    "\U0001F49B\U0001F49C\U0001F49A\U0001F499"
    "\U0001F493\U0001F498\U0001F49D\U0001FA77";

static regex_t *compile_reactions_regex(void)
{
    const char *prefix =
        "\\bgood\\s*bot\\b"
        "|\\bi\\s*(?:love|luv)\\s*(?:you|u|ya)\\b"
        "|\\b(?:love|luv)\\s*(?:you|u|ya)\\b"
        "|\\bily(?:sm)?\\b"
        "|\\bthank\\s*(?:you|u)\\b"
        "|\\b(?:thanks|thx|tysm|ty)\\b"
        "|<3+";

    size_t plen = strlen(prefix);
    size_t hlen = strlen(HEART_CHARS);
    char *full = (char *)malloc(plen + 2 + hlen + 2);
    if (!full) return NULL;
    memcpy(full, prefix, plen);
    full[plen] = '[';
    memcpy(full + plen + 1, HEART_CHARS, hlen);
    full[plen + 1 + hlen] = ']';
    full[plen + 1 + hlen + 1] = '\0';

    regex_t *re = (regex_t *)malloc(sizeof(regex_t));
    if (!re) { free(full); return NULL; }
    int rc = regcomp(re, full, REG_EXTENDED | REG_ICASE);
    free(full);
    if (rc != 0) { free(re); return NULL; }
    return re;
}

static regex_t *g_reactions_re = NULL;
static pthread_once_t g_reactions_once = PTHREAD_ONCE_INIT;

static void reactions_init(void)
{
    g_reactions_re = compile_reactions_regex();
}

/* PoP: reactions_detect @ agent/reactions.py:detect_reaction */
const char *reactions_detect(const char *text)
{
    if (!text || !text[0]) return NULL;

    pthread_once(&g_reactions_once, reactions_init);
    regex_t *re = g_reactions_re;
    if (!re) return NULL;

    int rc = regexec(re, text, 0, NULL, 0);
    return (rc == 0) ? "vibe" : NULL;
}

/* PoP: agent_reactions_u_detect_reaction @ agent/reactions.py:detect_reaction */
/* VIBE ("vibe") when _VIBE_RE.search(text) else None(""). Faithful hand-port of
 * the lexicon: ASCII-case-insensitive phrases with \b word edges and \s* gaps,
 * "<3+", and the heart/face codepoint class. INPUT IS HEX OF THE UTF-8 TEXT. */
static int _rx_is_word(unsigned c) {
    return (c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c>=0x80;
}
static unsigned _rx_lower(unsigned c) { return (c>='A'&&c<='Z') ? c+32 : c; }
static int _rx_word_at(const unsigned *cp, int n, int i, const char *w) {
    int k = 0;
    while (w[k]) {
        if (i + k >= n || _rx_lower(cp[i+k]) != (unsigned char)w[k]) return 0;
        k++;
    }
    return k;
}
/* lead_bound/tail_bound mirror where the pattern actually has \b: only the first
 * and last token of a phrase. Intermediate tokens are joined by \s* (which may be
 * EMPTY), so they carry no boundary at all. */
static int _rx_match_alts(const unsigned *cp, int n, int i, const char *const *alts, int na,
                          int lead_bound, int tail_bound) {
    if (lead_bound && i > 0 && _rx_is_word(cp[i-1])) return -1;
    for (int a = 0; a < na; a++) {
        int len = _rx_word_at(cp, n, i, alts[a]);
        if (len > 0) {
            int j = i + len;
            if (tail_bound && j < n && _rx_is_word(cp[j])) continue;
            return j;
        }
    }
    return -1;
}
static int _rx_skip_ws(const unsigned *cp, int n, int i) {
    while (i < n && (cp[i]==' '||cp[i]=='\t'||cp[i]=='\n'||cp[i]=='\r'||cp[i]=='\f'||cp[i]=='\v')) i++;
    return i;
}
static int _rx_emoji(unsigned c) {
    static const unsigned EM[] = {0x2764,0x2665,0x1F970,0x1F60D,0x1F618,0x1F495,0x1F496,0x1F497,
        0x1F49E,0x1F49B,0x1F49C,0x1F49A,0x1F499,0x1F493,0x1F498,0x1F49D,0x1FA77};
    for (int i = 0; i < (int)(sizeof(EM)/sizeof(EM[0])); i++) if (EM[i] == c) return 1;
    return 0;
}
char *agent_reactions_u_detect_reaction(const char *arg) {
    const char *h = arg ? arg : "";
    size_t cap = strlen(h)/2 + 2;
    unsigned *cp = malloc(sizeof(unsigned) * (cap ? cap : 1));
    int n = 0;
    for (const char *p = h; p[0] && p[1]; p += 2) {
        int hi = (p[0]>='0'&&p[0]<='9')?p[0]-'0':(p[0]>='a'&&p[0]<='f')?p[0]-'a'+10:(p[0]>='A'&&p[0]<='F')?p[0]-'A'+10:-1;
        int lo = (p[1]>='0'&&p[1]<='9')?p[1]-'0':(p[1]>='a'&&p[1]<='f')?p[1]-'a'+10:(p[1]>='A'&&p[1]<='F')?p[1]-'A'+10:-1;
        if (hi < 0 || lo < 0) break;
        unsigned char b = (unsigned char)(hi*16+lo);
        if (b < 0x80) { cp[n++] = b; }
        else {
            unsigned c; int len;
            if ((b & 0xe0) == 0xc0) { c = b & 0x1f; len = 2; }
            else if ((b & 0xf0) == 0xe0) { c = b & 0x0f; len = 3; }
            else if ((b & 0xf8) == 0xf0) { c = b & 0x07; len = 4; }
            else { c = b; len = 1; }
            for (int k = 1; k < len; k++) {
                const char *q = p + k*2;
                if (!q[0] || !q[1]) { c = 0xFFFFFFFFu; break; }
                int h2 = (q[0]>='0'&&q[0]<='9')?q[0]-'0':(q[0]>='a'&&q[0]<='f')?q[0]-'a'+10:(q[0]>='A'&&q[0]<='F')?q[0]-'A'+10:-1;
                int l2 = (q[1]>='0'&&q[1]<='9')?q[1]-'0':(q[1]>='a'&&q[1]<='f')?q[1]-'a'+10:(q[1]>='A'&&q[1]<='F')?q[1]-'A'+10:-1;
                if (h2 < 0 || l2 < 0) { c = 0xFFFFFFFFu; break; }
                c = (c << 6) | ((unsigned char)(h2*16+l2) & 0x3f);
            }
            cp[n++] = c;
            p += (size_t)(len-1)*2;
        }
    }
    static const char *T_GOOD[]={"good"}, *T_BOT[]={"bot"}, *T_I[]={"i"},
        *T_LOVE[]={"love","luv"}, *T_YOU[]={"you","u","ya"}, *T_ILY[]={"ilysm","ily"},
        *T_THANK[]={"thank"}, *T_THANKS[]={"thanks","thx","tysm","ty"};
    int hit = 0;
    for (int i = 0; i < n && !hit; i++) {
        int j;
        /* <3+ (no word boundaries) */
        if (cp[i] == '<' && i+1 < n && cp[i+1] == '3') { hit = 1; break; }
        if (_rx_emoji(cp[i])) { hit = 1; break; }
        if ((j = _rx_match_alts(cp, n, i, T_GOOD, 1, 1, 0)) >= 0) {
            j = _rx_skip_ws(cp, n, j);
            if (_rx_match_alts(cp, n, j, T_BOT, 1, 0, 1) >= 0) { hit = 1; break; }
        }
        if ((j = _rx_match_alts(cp, n, i, T_I, 1, 1, 0)) >= 0) {
            j = _rx_skip_ws(cp, n, j);
            int k2 = _rx_match_alts(cp, n, j, T_LOVE, 2, 0, 0);
            if (k2 >= 0) { k2 = _rx_skip_ws(cp, n, k2); if (_rx_match_alts(cp, n, k2, T_YOU, 3, 0, 1) >= 0) { hit = 1; break; } }
        }
        if ((j = _rx_match_alts(cp, n, i, T_LOVE, 2, 1, 0)) >= 0) {
            j = _rx_skip_ws(cp, n, j);
            if (_rx_match_alts(cp, n, j, T_YOU, 3, 0, 1) >= 0) { hit = 1; break; }
        }
        if (_rx_match_alts(cp, n, i, T_ILY, 2, 1, 1) >= 0) { hit = 1; break; }
        if ((j = _rx_match_alts(cp, n, i, T_THANK, 1, 1, 0)) >= 0) {
            j = _rx_skip_ws(cp, n, j);
            if (_rx_match_alts(cp, n, j, T_YOU, 2, 0, 1) >= 0) { hit = 1; break; }
        }
        if (_rx_match_alts(cp, n, i, T_THANKS, 4, 1, 1) >= 0) { hit = 1; break; }
    }
    free(cp);
    return strdup(hit ? "vibe" : "");
}

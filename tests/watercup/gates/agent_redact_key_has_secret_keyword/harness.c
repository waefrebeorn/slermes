/* gate: agent_redact_key_has_secret_keyword — traced copy of
 * src/agent/redact_helpers.c::agent_redact_key_has_secret_keyword (+ its
 * static helpers next_keyword_match / _is_word_start / _is_word_end).
 * LIVE twin: agent/redact.py::_key_has_secret_keyword.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty key -> false
 *   LOOP over key chars; B2: has-alpha && all-upper -> legacy short-circuit true
 *   LOOP over keyword matches:
 *     B3: word-start && word-end -> true
 *   B4: exhausted -> false
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "watercup.h"

static const char *const CPREFIX[] = {"api","auth","access","refresh","session","secret",NULL};
static const char *const CSUFFIX[] = {"key","token",NULL};
static const char *const SIMPLE[]  = {"token","secret","passwd","password","credential","auth",NULL};

static int cincmp(const char *s, const char *lit, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)lit[i])) return 1;
    return 0;
}

static bool is_word_start(const char *s, size_t i) {
    if (i == 0) return true;
    char prev = s[i-1], cur = s[i];
    if (!isalpha((unsigned char)prev)) return true;
    if (isupper((unsigned char)cur) && islower((unsigned char)prev)) return true;
    if (isupper((unsigned char)cur) && isupper((unsigned char)prev) &&
        i + 1 < strlen(s) && islower((unsigned char)s[i+1])) return true;
    return false;
}
static bool is_word_end(const char *s, size_t j, bool allow_plural) {
    size_t len = strlen(s);
    if (j >= len) return true;
    char cur = s[j];
    if (!isalpha((unsigned char)cur)) return true;
    if (isupper((unsigned char)cur) && islower((unsigned char)s[j-1])) return true;
    if (allow_plural && (cur == 's' || cur == 'S'))
        return is_word_end(s, j + 1, false);
    return false;
}
/* returns match start or -1 */
static long next_keyword_match(const char *s, size_t from, size_t *mlen) {
    size_t len = strlen(s);
    for (size_t i = from; i < len; i++) {
        for (int p = 0; CPREFIX[p]; p++) {
            size_t pl = strlen(CPREFIX[p]);
            if (i + pl > len || cincmp(s + i, CPREFIX[p], pl)) continue;
            size_t pos = i + pl;
            if (pos < len && (s[pos]==' '||s[pos]=='_'||s[pos]=='.'||s[pos]=='-')) pos++;
            for (int suf = 0; CSUFFIX[suf]; suf++) {
                size_t sl = strlen(CSUFFIX[suf]);
                if (pos + sl <= len && !cincmp(s+pos, CSUFFIX[suf], sl)) { *mlen = pos+sl-i; return (long)i; }
            }
        }
        for (int k = 0; SIMPLE[k]; k++) {
            size_t kl = strlen(SIMPLE[k]);
            if (i + kl <= len && !cincmp(s+i, SIMPLE[k], kl)) { *mlen = kl; return (long)i; }
        }
    }
    return -1;
}

static bool wc_key_has_secret_keyword(const char *key) {
    WCUP_BEGIN("key_has_secret_keyword");
    bool result = false;
    if (!key || !*key) { WCUP_BRANCH(1); goto out; }   /* B1 */
    WCUP_BRANCH(0);
    {
        bool all_upper = true, has_alpha = false;
        for (const char *p = key; *p; p++) {
            WCUP_LOOP_TICK();
            if (isalpha((unsigned char)*p)) {
                has_alpha = true;
                if (!isupper((unsigned char)*p)) { all_upper = false; break; }
            }
        }
        if (has_alpha && all_upper) {          /* B2 legacy short-circuit */
            WCUP_BRANCH(1);
            result = true;
            goto out;
        }
        WCUP_BRANCH(0);
    }
    {
        size_t from = 0, mlen;
        long m;
        bool found = false;
        while ((m = next_keyword_match(key, from, &mlen)) >= 0) {
            WCUP_LOOP_TICK();
            if (is_word_start(key, (size_t)m) && is_word_end(key, (size_t)m + mlen, true)) {
                WCUP_BRANCH(1);                /* B3 */
                found = true;
                break;
            }
            WCUP_BRANCH(0);
            from = (size_t)m + 1;
        }
        result = found;                        /* B4 */
        WCUP_BRANCH(result);
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("key_has_secret_keyword");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s key_has_secret_keyword <key>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_key_has_secret_keyword(argv[i]);
    return 0;
}

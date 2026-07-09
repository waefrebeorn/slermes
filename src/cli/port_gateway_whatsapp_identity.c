/*
 * port_gateway_whatsapp_identity.c — C port of gateway/whatsapp_identity.py
 *
 * to_whatsapp_jid(value): normalize an *outbound* WhatsApp target to a
 * bridge-safe JID. Faithful to LIVE Python:
 *   - empty/whitespace -> ""
 *   - strip + device-suffix (user:device@domain -> user@domain)
 *   - already has '@' -> unchanged
 *   - bare phone (^+\d\s().-+$) -> digits@s.whatsapp.net
 *   - else unchanged
 *
 * _BARE_PHONE_RE = ^\+?[\d\s().\-]+$ (Python `re.fullmatch`)
 *
 * Verified byte-equal to LIVE Python via tests/sta_oracle_whatsapp_identity.py.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>

/* PoP: gateway_whatsapp_to_jid @ gateway/whatsapp_identity.py:to_whatsapp_jid */
char *gateway_whatsapp_to_jid(const char *value) {
    if (!value) return strdup("");
    /* strip */
    const char *p = value;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    char *tmp = strdup(p);
    if (!tmp) return NULL;
    size_t len = strlen(tmp);
    while (len > 0 && (tmp[len-1] == ' ' || tmp[len-1] == '\t' ||
                       tmp[len-1] == '\n' || tmp[len-1] == '\r')) {
        tmp[--len] = '\0';
    }
    if (len == 0) { free(tmp); return strdup(""); }

    /* strip a leading '+' for the device-split handling below (Python .replace("+","",1)) */
    const char *norm = tmp;
    if (norm[0] == '+') norm = norm + 1;

    char *result = NULL;

    /* device-suffix: "user:device@domain" -> "user@domain" (only if both ':' and '@') */
    const char *at = strchr(norm, '@');
    const char *colon = strchr(norm, ':');
    if (at && colon && colon < at) {
        /* prefix = before ':', domain = after '@' */
        size_t prefix_len = (size_t)(colon - norm);
        const char *domain = at + 1;
        size_t built = prefix_len + 1 + strlen(domain) + 1; /* prefix + '@' + domain + NUL */
        result = malloc(built);
        if (!result) { free(tmp); return NULL; }
        memcpy(result, norm, prefix_len);
        result[prefix_len] = '@';
        strcpy(result + prefix_len + 1, domain);
        free(tmp);
        return result;
    }

    /* already a fully-qualified JID */
    if (strchr(norm, '@')) {
        char *r = strdup(norm);
        free(tmp);
        return r;
    }

    /* bare-phone test: ^\+?[\d\s().\-]+$ on the stripped (no leading +) norm */
    bool is_phone = (norm[0] != '\0');
    for (const char *q = norm; *q; q++) {
        char c = *q;
        if (!(c == '+' || (c >= '0' && c <= '9') || c == ' ' || c == '(' ||
              c == ')' || c == '.' || c == '-')) {
            is_phone = false;
            break;
        }
    }
    if (is_phone) {
        /* digits = re.sub(r"\D+", "", normalized) */
        char digits[256];
        size_t d = 0;
        for (const char *q = tmp; *q && d < sizeof(digits)-1; q++) {
            if (*q >= '0' && *q <= '9') digits[d++] = *q;
        }
        digits[d] = '\0';
        if (digits[0] != '\0') {
            size_t built = strlen(digits) + strlen("@s.whatsapp.net") + 1;
            result = malloc(built);
            if (!result) { free(tmp); return NULL; }
            snprintf(result, built, "%s@s.whatsapp.net", digits);
            free(tmp);
            return result;
        }
    }

    char *r = strdup(norm);
    free(tmp);
    return r;
}

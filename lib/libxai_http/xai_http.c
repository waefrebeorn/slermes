/*
 * xai_http.c — Shared helpers for direct xAI HTTP integrations.
 *
 * Port of Python tools/xai_http.py.
 *
 * Provides simple environment-based credential resolution for xAI
 * HTTP endpoints (images, TTS, STT, web search, etc.).
 */

#include "xai_http.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Port of Python tools/xai_http.py:has_xai_credentials().
 * Cheap probe — True when xAI credentials are *likely* usable. No locks, no
 * network refresh. Resolution order: XAI_API_KEY env (plus the historical
 * HERMES_XAI_API_KEY alias), then <home>/.hermes/auth.json providers.
 * xai-oauth.tokens.access_token, then credential_pool["xai-oauth"] entries
 * with a non-empty access_token. Returns false on any parse failure so a
 * corrupted store cannot block availability scans.
 */
bool has_xai_credentials(void)
{
    const char *key = getenv("XAI_API_KEY");
    if (!key || key[0] == 0) key = getenv("HERMES_XAI_API_KEY");
    if (key && key[0] != 0) return true;

    char path[4096];
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) return false;
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);
    FILE *fh = fopen(path, "rb");
    if (!fh) return false;
    fseek(fh, 0, SEEK_END);
    long n = ftell(fh);
    fseek(fh, 0, SEEK_SET);
    if (n < 0) { fclose(fh); return false; }
    char *text = malloc((size_t)n + 1);
    if (!text) { fclose(fh); return false; }
    size_t rd = fread(text, 1, (size_t)n, fh);
    text[rd] = 0;
    fclose(fh);

    char *js = text;   /* strip UTF-8 BOM (Python utf-8-sig) */
    if ((unsigned char)js[0] == 0xEF && (unsigned char)js[1] == 0xBB &&
        (unsigned char)js[2] == 0xBF)
        js += 3;

    /* Minimal token-presence scan (this lib stays JSON-free by design):
     * find any "access_token": "<non-empty>" member. Both the providers
     * xai-oauth singleton and credential_pool entries share the key shape. */
    bool found = false;
    const char *p = js;
    while (!found && (p = strstr(p, "\"access_token\"")) != NULL) {
        p += strlen("\"access_token\"");
        const char *colon = strchr(p, ':');
        if (!colon) break;
        colon++;
        while (*colon == ' ' || *colon == '\t') colon++;
        if (*colon == '"') {
            colon++;
            if (*colon != '"')
                found = true;
        }
    }
    free(text);
    return found;
}

bool xai_get_api_key(char out_key[XAI_API_KEY_MAX])
{
    if (!out_key) return false;

    const char *key = getenv("HERMES_XAI_API_KEY");
    if (!key || key[0] == '\0') {
        out_key[0] = '\0';
        return false;
    }

    snprintf(out_key, XAI_API_KEY_MAX, "%s", key);
    return true;
}

void xai_get_base_url(char out_url[XAI_BASE_URL_MAX])
{
    if (!out_url) return;

    const char *url = getenv("HERMES_XAI_BASE_URL");
    if (url && url[0] != '\0') {
        snprintf(out_url, XAI_BASE_URL_MAX, "%s", url);
    } else {
        snprintf(out_url, XAI_BASE_URL_MAX, "%s", XAI_DEFAULT_BASE_URL);
    }
}

const char *xai_user_agent(void)
{
    return XAI_USER_AGENT;
}

/* gate: api_error_summary_ladder — traced copy of the FIXED
 * src/agent/api_error_summary.c::summarize_api_error
 * (LIVE twin: run_agent.py::AIAgent._summarize_api_error).
 *
 * Input tags (argv):
 *   "NET:<text>"   network/DNS resolution failure message
 *   "VE:<text>"    ValueError-style malformed streaming response
 *   "HTML:<text>"  Cloudflare/proxy HTML page (may embed literal "HTTP <sc>"
 *                  and "Cloudflare Ray ID: <strong>..</strong>")
 *   "BODY:<json>"  SDK JSON body error
 *   "RAW:<text>"   plain error → truncated fallback
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: network resolution marker found
 *   B2: malformed streaming response
 *   B3: HTML error page
 *   B4 (html): title tag found      B5 (html): ray id found
 *   B6: body parses to a dict with a usable message
 *   VSTR(label) in {offline, malformed, html, json_msg, fallback}
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "hermes_json.h"
#include "watercup.h"

static int wc_json_body_message(const char *raw, char *out, int outsz);

static const char *NET_MARKERS[] = {
    "temporary failure in name resolution",
    "name or service not known",
    "nodename nor servname provided, or not known",
    "getaddrinfo failed",
    "no address associated with hostname",
    "network is unreachable",
    NULL
};

static char *wc_summarize(const char *raw_error)
{
    WCUP_BEGIN("api_error_summary_ladder");
    const char *label = "fallback";

    /* B1: network marker */
    {
        int hit = 0;
        char low[8192];
        size_t rl = strlen(raw_error);
        for (size_t i = 0; i <= rl && i < sizeof(low); i++)
            low[i] = (char)tolower((unsigned char)raw_error[i]);
        for (int i = 0; NET_MARKERS[i]; i++)
            if (strstr(low, NET_MARKERS[i])) { hit = 1; break; }
        WCUP_BRANCH(hit);
        if (hit) { label = "offline"; goto done; }
    }

    /* B2: malformed streaming response */
    if (strstr(raw_error, "expected ident at line")) {
        WCUP_BRANCH(1);
        label = "malformed";
        goto done;
    }
    WCUP_BRANCH(0);

    /* B3: HTML page */
    if (strstr(raw_error, "<!DOCTYPE") || strstr(raw_error, "<html")) {
        WCUP_BRANCH(1);
        label = "html";
        const char *ts = strstr(raw_error, "<title");
        WCUP_BRANCH(ts != NULL);            /* B4 */
        const char *ray = strstr(raw_error, "Ray ID:");
        WCUP_BRANCH(ray != NULL);           /* B5 */
        goto done;
    }
    WCUP_BRANCH(0);

    /* B6: JSON body dict with usable message */
    {
        char msg[512];
        int got = wc_json_body_message(raw_error, msg, (int)sizeof msg);
        WCUP_BRANCH(got);
        if (got) { label = "json_msg"; goto done; }
    }

done:
    WCUP_VALSTR(label);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("api_error_summary_ladder");
    return NULL;
}

/* Minimal json-body probe mirroring the LIVE lookup:
 * body.error.message when error is an object, else body.message. */
static const char *jobj_str(json_t *o, const char *key)
{
    if (!o || o->type != JSON_OBJECT) return NULL;
    json_t *v = json_object_get(o, key);
    return (v && v->type == JSON_STRING && v->str_val && v->str_val[0])
        ? v->str_val : NULL;
}

static int wc_json_body_message(const char *raw, char *out, int outsz)
{
    json_t *root = json_parse(raw, NULL);
    if (!root) return 0;
    if (root->type != JSON_OBJECT) { json_free(root); return 0; }
    const char *msg = jobj_str(json_object_get(root, "error"), "message");
    if (!msg) msg = jobj_str(root, "message");
    if (!msg) { json_free(root); return 0; }
    snprintf(out, (size_t)outsz, "%s", msg);
    json_free(root);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *in = argv[i];
        /* strip the 5-char tag prefix ("NET:"/"VE:"/"HTML:"/"BODY:"/"RAW:") */
        const char *colon = strchr(in, ':');
        wc_summarize(colon ? colon + 1 : in);
    }
    return 0;
}

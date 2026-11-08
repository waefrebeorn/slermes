/*
 * port_hosted_room_execution_policy.c — Port of Python:
 * gateway/hosted_room_execution_policy.py (validation + digest half).
 *
 * RoomExecutionPolicy is a frozen dataclass whose from_mapping() is a pure
 * validator: exact field set, version gate, identifier shape checks, a sorted
 * unique toolset list that must include "bot_room", an approval-mode enum, a
 * bounded iteration count, and a sha256 over the canonical JSON of the
 * unsigned policy. All of that is reproduced here; the served-profile config
 * resolution is not (it needs the live gateway config).
 */

#include "port_hosted_room_execution_policy.h"
#include "hermes_json_strutil.h"
#include "hermes_strutil.h"
#include "crypto.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POLICY_ERR_FIELDS   "execution policy fields are invalid"
#define POLICY_ERR_VERSION  "execution policy version is unsupported"
#define POLICY_ERR_TOOLSETS "enabled_toolsets are invalid"
#define POLICY_ERR_APPROVAL "approval_mode is invalid"
#define POLICY_ERR_ITERS    "max_iterations is invalid"
#define POLICY_ERR_DIGEST   "policy_digest does not match the execution policy"

static const char *const POLICY_FIELDS[] = {
    "version", "target_profile", "enabled_toolsets", "approval_mode",
    "max_iterations", "policy_digest",
};

static char *dup_printf(const char *fmt, const char *a)
{
    size_t cap = strlen(fmt) + (a ? strlen(a) : 0) + 8;
    char *out = (char *)malloc(cap);
    if (out) snprintf(out, cap, fmt, a ? a : "");
    return out;
}

static int set_error(char **err, const char *message)
{
    if (err) *err = dup_printf("%s", message);
    return -1;
}

/* Python identifier(): strip, 1..128 chars, ^[A-Za-z0-9][A-Za-z0-9._:-]*$ */
static int identifier_ok(const char *s)
{
    if (!s) return 0;
    size_t n = strlen(s);
    if (n == 0 || n > 128) return 0;
    if (!isalnum((unsigned char)s[0])) return 0;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!(isalnum(c) || c == '.' || c == '_' || c == ':' || c == '-'))
            return 0;
    }
    return 1;
}



/* Python str(value or "") for a JSON node: strings verbatim, numbers rendered,
 * null/absent as "" (falsey -> ""), everything else as JSON text. */


/* PoP: hep_policy_digest @ gateway/hosted_room_execution_policy.py:_policy_digest */
char *hep_policy_digest(const char *target_profile,
                        const char *const *toolsets, int n_toolsets,
                        const char *approval_mode, long long max_iterations)
{
    /* compact_json(unsigned): sorted keys, separators (",", ":"), ensure_ascii.
     * Every value here is an ASCII identifier or a number, so no escaping is
     * needed beyond JSON string quoting. */
    size_t cap = 256;
    for (int i = 0; i < n_toolsets; i++)
        cap += (toolsets[i] ? strlen(toolsets[i]) : 0) + 4;
    cap += (target_profile ? strlen(target_profile) : 0)
         + (approval_mode ? strlen(approval_mode) : 0) + 128;
    char *canon = (char *)malloc(cap);
    if (!canon) return NULL;

    size_t o = 0;
    o += (size_t)snprintf(canon + o, cap - o,
        "{\"approval_mode\":\"%s\",\"enabled_toolsets\":[",
        approval_mode ? approval_mode : "");
    for (int i = 0; i < n_toolsets; i++)
        o += (size_t)snprintf(canon + o, cap - o, "%s\"%s\"",
                              i ? "," : "", toolsets[i] ? toolsets[i] : "");
    o += (size_t)snprintf(canon + o, cap - o,
        "],\"max_iterations\":%lld,\"target_profile\":\"%s\",\"version\":%d}",
        max_iterations, target_profile ? target_profile : "", HEP_POLICY_VERSION);

    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)canon, strlen(canon), digest);
    free(canon);

    char *hex = (char *)malloc((size_t)CRYPTO_SHA256_LEN * 2 + 1);
    if (!hex) return NULL;
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < CRYPTO_SHA256_LEN; i++) {
        hex[i * 2]     = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 0x0f];
    }
    hex[CRYPTO_SHA256_LEN * 2] = '\0';
    return hex;
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void policy_clear(hep_policy_t *p)
{
    if (!p) return;
    free(p->target_profile);
    for (int i = 0; i < p->n_toolsets; i++) free(p->enabled_toolsets[i]);
    free(p->enabled_toolsets);
    free(p->approval_mode);
    free(p->policy_digest);
    memset(p, 0, sizeof(*p));
}

/* PoP: hep_policy_from_mapping @ gateway/hosted_room_execution_policy.py:from_mapping */
int hep_policy_from_mapping(const char *json, hep_policy_t *out, char **err)
{
    if (err) *err = NULL;
    if (!out) return set_error(err, POLICY_ERR_FIELDS);
    memset(out, 0, sizeof(*out));

    json_t *doc = json ? json_parse(json, NULL) : NULL;
    if (!doc || doc->type != JSON_OBJECT) {
        if (doc) json_free(doc);
        return set_error(err, POLICY_ERR_FIELDS);
    }

    /* set(value) must equal _POLICY_FIELDS exactly. */
    size_t nfields = sizeof(POLICY_FIELDS) / sizeof(POLICY_FIELDS[0]);
    if (doc->c.count != nfields) {
        json_free(doc);
        return set_error(err, POLICY_ERR_FIELDS);
    }
    for (size_t i = 0; i < nfields; i++) {
        if (!json_obj_get(doc, POLICY_FIELDS[i])) {
            json_free(doc);
            return set_error(err, POLICY_ERR_FIELDS);
        }
    }

    json_t *version = json_obj_get(doc, "version");
    if (!version || version->type != JSON_NUMBER ||
        version->num_val != (double)HEP_POLICY_VERSION) {
        json_free(doc);
        return set_error(err, POLICY_ERR_VERSION);
    }

    /* target_profile: _identifier(value) stringifies first. */
    char *raw_profile = json_node_to_str(json_obj_get(doc, "target_profile"));
    char *profile = str_strip_dup(raw_profile ? raw_profile : "");
    free(raw_profile);
    if (!identifier_ok(profile)) {
        free(profile);
        json_free(doc);
        return set_error(err, "target_profile is invalid");
    }

    json_t *toolsets = json_obj_get(doc, "enabled_toolsets");
    if (!toolsets || toolsets->type != JSON_ARRAY ||
        toolsets->c.count < 1 || toolsets->c.count > HEP_MAX_TOOLSETS) {
        free(profile);
        json_free(doc);
        return set_error(err, POLICY_ERR_TOOLSETS);
    }

    int n = (int)toolsets->c.count;
    char **list = (char **)calloc((size_t)n, sizeof(char *));
    if (!list) { free(profile); json_free(doc); return -1; }
    for (int i = 0; i < n; i++) {
        char *raw = json_node_to_str(json_get(toolsets, (size_t)i));
        char *item = str_strip_dup(raw ? raw : "");
        free(raw);
        if (!identifier_ok(item)) {
            free(item);
            for (int j = 0; j < i; j++) free(list[j]);
            free(list);
            free(profile);
            json_free(doc);
            return set_error(err, "enabled_toolset is invalid");
        }
        list[i] = item;
    }
    qsort(list, (size_t)n, sizeof(char *), cmp_str);

    int has_bot_room = 0, dup = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(list[i], "bot_room") == 0) has_bot_room = 1;
        if (i && strcmp(list[i - 1], list[i]) == 0) dup = 1;
    }
    if (dup || !has_bot_room) {
        for (int i = 0; i < n; i++) free(list[i]);
        free(list);
        free(profile);
        json_free(doc);
        return set_error(err, POLICY_ERR_TOOLSETS);
    }

    char *raw_mode = json_node_to_str(json_obj_get(doc, "approval_mode"));
    char *mode = str_strip_dup(raw_mode ? raw_mode : "");
    free(raw_mode);
    if (mode) for (char *p = mode; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (!mode || (strcmp(mode, "manual") != 0 && strcmp(mode, "smart") != 0 &&
                  strcmp(mode, "off") != 0)) {
        free(mode);
        for (int i = 0; i < n; i++) free(list[i]);
        free(list);
        free(profile);
        json_free(doc);
        return set_error(err, POLICY_ERR_APPROVAL);
    }

    json_t *iters = json_obj_get(doc, "max_iterations");
    if (!iters || iters->type != JSON_NUMBER) {
        free(mode);
        for (int i = 0; i < n; i++) free(list[i]);
        free(list);
        free(profile);
        json_free(doc);
        return set_error(err, POLICY_ERR_ITERS);
    }
    double d = iters->num_val;
    if (d < 1.0 || d > (double)HEP_MAX_ITERATIONS || d != (double)(long long)d) {
        free(mode);
        for (int i = 0; i < n; i++) free(list[i]);
        free(list);
        free(profile);
        json_free(doc);
        return set_error(err, POLICY_ERR_ITERS);
    }
    long long max_iters = (long long)d;

    char *computed = hep_policy_digest(profile, (const char *const *)list, n,
                                       mode, max_iters);
    char *raw_digest = json_node_to_str(json_obj_get(doc, "policy_digest"));
    char *supplied = str_strip_dup(raw_digest ? raw_digest : "");
    free(raw_digest);
    if (supplied) for (char *p = supplied; *p; p++) *p = (char)tolower((unsigned char)*p);
    int digest_ok = computed && supplied && strcmp(computed, supplied) == 0;
    free(computed);
    if (!digest_ok) {
        free(supplied);
        free(mode);
        for (int i = 0; i < n; i++) free(list[i]);
        free(list);
        free(profile);
        json_free(doc);
        return set_error(err, POLICY_ERR_DIGEST);
    }
    json_free(doc);

    out->version = HEP_POLICY_VERSION;
    out->target_profile = profile;
    out->enabled_toolsets = list;
    out->n_toolsets = n;
    out->approval_mode = mode;
    out->max_iterations = max_iters;
    out->policy_digest = supplied;
    return 0;
}

/* PoP: hep_policy_as_mapping @ gateway/hosted_room_execution_policy.py:as_mapping */
json_t *hep_policy_as_mapping(const hep_policy_t *policy)
{
    if (!policy) return NULL;
    json_t *obj = json_object();
    if (!obj) return NULL;
    json_set(obj, "version", json_number((double)policy->version));
    json_set(obj, "target_profile", json_string(policy->target_profile ? policy->target_profile : ""));
    json_t *arr = json_array();
    for (int i = 0; i < policy->n_toolsets; i++)
        json_append(arr, json_string(policy->enabled_toolsets[i]));
    json_set(obj, "enabled_toolsets", arr);
    json_set(obj, "approval_mode", json_string(policy->approval_mode ? policy->approval_mode : ""));
    json_set(obj, "max_iterations", json_number((double)policy->max_iterations));
    json_set(obj, "policy_digest", json_string(policy->policy_digest ? policy->policy_digest : ""));
    return obj;
}

void hep_policy_free(hep_policy_t *policy)
{
    policy_clear(policy);
}

/*
 * release_channels.c - C home for upstream Hermes hermes_cli/release_channels.py.
 * PoP: w15_releasecha_5f3af5_validate_name @ hermes_cli/release_channels.py:validate_name
 * PoP: w15_releasecha_5f3af5_validate_repository @ hermes_cli/release_channels.py:validate_repository
 * PoP: w15_releasecha_5f3af5_require_sha256 @ hermes_cli/release_channels.py:require_sha256
 * PoP: w15_releasecha_5f3af5_require_commit @ hermes_cli/release_channels.py:require_commit
 * PoP: w15_releasecha_5f3af5_build_prefix @ hermes_cli/release_channels.py:build_prefix
 * PoP: w15_releasecha_5f3af5_channel_key @ hermes_cli/release_channels.py:channel_key
 * PoP: w15_releasecha_5f3af5_package_versions @ hermes_cli/release_channels.py:package_versions
 * PoP: w15_releasecha_5f3af5_artifact_key @ hermes_cli/release_channels.py:artifact_key
 * PoP: w15_releasecha_5f3af5_head @ hermes_cli/release_channels.py:_head
 * PoP: w15_releasecha_5f3af5_validate_identity @ hermes_cli/release_channels.py:validate_identity
 *
 * Pure channel wire validators. Failures are reported as
 * "ChannelError: <message>" strings, matching raise ChannelError on the Python side.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_releasecha_5f3af5.h"

#define MAX_SEQUENCE 4294967295LL

static json_t *err(const char *label) {
    char b[128];
    snprintf(b, sizeof(b), "ChannelError: Invalid %s", label);
    return json_string(b);
}

static int is_lower_alnum(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
static int is_alpha(unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static int is_alnum(unsigned char c) { return is_alpha(c) || (c >= '0' && c <= '9'); }
static int is_hexlow(unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }

static int n_hexlower(const char *s, size_t want) {
    if (!s || strlen(s) != want) return 0;
    for (size_t i = 0; i < want; i++) if (!is_hexlow((unsigned char)s[i])) return 0;
    return 1;
}

static int reserved(const char *s) {
    static const char *const r[] = {"con", "prn", "aux", "nul",
        "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
        "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9", NULL};
    for (int i = 0; r[i]; i++) if (!strcmp(s, r[i])) return 1;
    return 0;
}

/* [a-z0-9]+(?:-[a-z0-9]+)* */
static int valid_name(const char *s) {
    if (!s || !s[0]) return 0;
    size_t n = strlen(s);
    if (n > 32 || reserved(s)) return 0;
    size_t i = 0;
    while (i < n) {
        if (!is_lower_alnum((unsigned char)s[i])) return 0;
        while (i < n && is_lower_alnum((unsigned char)s[i])) i++;
        if (i == n) break;
        if (s[i] != '-') return 0;
        i++;
        if (i == n || !is_lower_alnum((unsigned char)s[i])) return 0;
    }
    return 1;
}

/* repository regex: alnum, alnum/_/-, slash, alnum, alnum/_/./- */
static int valid_repository(const char *s) {
    if (!s || !is_alnum((unsigned char)s[0])) return 0;
    const char *p = s;
    while (*p && (is_alnum((unsigned char)*p) || *p == '_' || *p == '-')) p++;
    if (*p != '/') return 0;
    p++;
    if (!is_alnum((unsigned char)*p)) return 0;
    p++;
    while (*p && (is_alnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-')) p++;
    return *p == 0;
}

struct json_t *w15_releasecha_5f3af5_validate_name(const struct json_t *value) {
    if (!value || value->type != JSON_STRING || !valid_name(value->str_val ? value->str_val : ""))
        return err("channel name");
    return json_copy(value);
}

struct json_t *w15_releasecha_5f3af5_validate_repository(const struct json_t *value) {
    if (!value || value->type != JSON_STRING || !valid_repository(value->str_val ? value->str_val : ""))
        return err("repository");
    return json_copy(value);
}

struct json_t *w15_releasecha_5f3af5_require_sha256(const struct json_t *value) {
    if (!value || value->type != JSON_STRING || !n_hexlower(value->str_val ? value->str_val : "", 64))
        return err("SHA256");
    return json_copy(value);
}

struct json_t *w15_releasecha_5f3af5_require_commit(const struct json_t *value) {
    if (!value || value->type != JSON_STRING || !n_hexlower(value->str_val ? value->str_val : "", 40))
        return err("commit");
    return json_copy(value);
}

struct json_t *w15_releasecha_5f3af5_build_prefix(const struct json_t *value) {
    if (!value || value->type != JSON_STRING || !n_hexlower(value->str_val ? value->str_val : "", 32))
        return err("build ID");
    char b[128];
    snprintf(b, sizeof(b), "releases/channel-builds/%s/", value->str_val);
    return json_string(b);
}

struct json_t *w15_releasecha_5f3af5_channel_key(const struct json_t *value) {
    json_t *name = w15_releasecha_5f3af5_validate_name(value);
    if (name->type == JSON_STRING && strstr(name->str_val, "ChannelError") == name->str_val) return name;
    char b[160];
    snprintf(b, sizeof(b), "releases/channels/%s.json", name->str_val ? name->str_val : "");
    json_free(name);
    return json_string(b);
}

/* sequence arrives as a decimal string (64-bit ints are not passed as JSON doubles) */
struct json_t *w15_releasecha_5f3af5_package_versions(const char *sequence_decimal) {
    char *end = NULL;
    long long seq = strtoll(sequence_decimal ? sequence_decimal : "", &end, 10);
    if (!sequence_decimal || !sequence_decimal[0] || end == sequence_decimal || *end ||
        seq < 1 || seq > MAX_SEQUENCE) return err("sequence");
    char a[64], b[96];
    snprintf(a, sizeof(a), "0.0.%lld", seq);
    snprintf(b, sizeof(b), "0.%lld.%lld.0", seq / 65536, seq % 65536);
    json_t *r = json_array();
    json_append(r, json_string(a));
    json_append(r, json_string(b));
    return r;
}

struct json_t *w15_releasecha_5f3af5_artifact_key(const struct json_t *value) {
    if (!value || value->type != JSON_STRING) return err("artifact key");
    const char *s = value->str_val ? value->str_val : "";
    if (!s[0]) return err("artifact key");
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!(is_alnum(c) || c == '_' || c == '.' || c == '/' || c == '+' || c == '-')) return err("artifact key");
    }
    if (strncmp(s, "releases/", 9)) return err("artifact key");
    const char *p = s;
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t n = slash ? (size_t)(slash - p) : strlen(p);
        if (n == 0 || (n == 1 && p[0] == '.') || (n == 2 && p[0] == '.' && p[1] == '.'))
            return err("artifact key");
        if (p[n - 1] == '.') return err("artifact key");
        char base[512];
        size_t k = 0;
        while (k < n && p[k] != '.') { base[k] = p[k]; k++; }
        base[k] = 0;
        for (char *q = base; *q; q++) *q = (char)((*q >= 'A' && *q <= 'Z') ? *q + 32 : *q);
        if (reserved(base)) return err("artifact key");
        if (!slash) break;
        p = slash + 1;
    }
    return json_copy(value);
}

/* _head: None -> null; otherwise build_prefix + sequence + manifestKey + sha256 */
struct json_t *w15_releasecha_5f3af5_head(const struct json_t *value) {
    if (!value || value->type == JSON_NULL) return json_null();
    if (value->type != JSON_OBJECT) return err("channel head");
    json_t *build = json_obj_get(value, "buildId");
    json_t *prefix = w15_releasecha_5f3af5_build_prefix(build);
    if (strstr(prefix->str_val ? prefix->str_val : "", "ChannelError") == prefix->str_val) {
        json_free(prefix);
        return err("channel head");
    }
    char *prefix_copy = (char *)malloc(strlen(prefix->str_val) + 1);
    if (prefix_copy) strcpy(prefix_copy, prefix->str_val); /* copy out before freeing prefix */
    json_free(prefix);
    json_t *seq = json_obj_get(value, "sequence");
    char seqbuf[64];
    if (!seq || seq->type != JSON_NUMBER || seq->num_val != (double)(long long)seq->num_val ||
        seq->num_val < 1 || seq->num_val > (double)MAX_SEQUENCE) { free(prefix_copy); return err("head sequence"); }
    snprintf(seqbuf, sizeof(seqbuf), "%lld", (long long)seq->num_val);
    json_t *seqres = w15_releasecha_5f3af5_package_versions(seqbuf);
    (void)seqres; /* validation only */
    json_t *mk = json_obj_get(value, "manifestKey");
    char expect[256];
    snprintf(expect, sizeof(expect), "%sbuild.json", prefix_copy ? prefix_copy : "");
    free(prefix_copy);
    if (!mk || mk->type != JSON_STRING || strcmp(mk->str_val ? mk->str_val : "", expect))
        return err("channel manifest key");
    json_t *sha = w15_releasecha_5f3af5_require_sha256(json_obj_get(value, "sha256"));
    if (strstr(sha->str_val ? sha->str_val : "", "ChannelError") == sha->str_val) { json_free(sha); return err("SHA256"); }
    json_free(sha);
    return json_copy(value);
}

static int match_shape(const char *s, int first_alpha, int min_total, int max_total,
                       int allow_space, int allow_digit, int allow_dash, int allow_dot, int allow_under) {
    if (!s || !s[0]) return 0;
    size_t n = strlen(s);
    if (n < (size_t)min_total || n > (size_t)max_total) return 0;
    unsigned char c0 = (unsigned char)s[0];
    if (first_alpha ? !is_alpha(c0) : !(c0 >= 'a' && c0 <= 'z')) return 0;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (is_alnum(c)) continue;
        if (allow_space && c == ' ') continue;
        if (allow_digit && c >= '0' && c <= '9') continue;
        if (allow_dash && c == '-') continue;
        if (allow_dot && c == '.') continue;
        if (allow_under && c == '_') continue;
        return 0;
    }
    return 1;
}

struct json_t *w15_releasecha_5f3af5_validate_identity(const struct json_t *value) {
    if (!value || value->type != JSON_OBJECT) return err("channel identity");
    static const struct {
        const char *field;
        const char *label;
        int first_alpha; int min_total; int max_total;
        int space; int dash; int dot; int under;
    } specs[] = {
        {"token", "identity token", 0, 16, 16, 0, 0, 0, 0},
        {"displayName", "identity display name", 1, 1, 80, 1, 1, 1, 1},
        {"appNamePascal", "appNamePascal", 1, 1, 64, 0, 0, 0, 0},
        {"artifactNamePascal", "artifactNamePascal", 1, 1, 64, 0, 0, 0, 0},
        {"appId", "app ID", 0, 3, 128, 0, 1, 1, 0},
        {"msixAppIdWithOrg", "MSIX identity", 1, 3, 50, 0, 1, 1, 0},
        {"cliName", "CLI name", 0, 1, 64, 0, 1, 0, 0},
        {"windowsExecutableName", "Windows executable", 1, 1, 80, 1, 1, 1, 1},
        {NULL, NULL, 0, 0, 0, 0, 0, 0, 0},
    };
    /* Python validates the token first */
    {
        json_t *tok0 = json_obj_get(value, "token");
        if (!tok0 || tok0->type != JSON_STRING || !n_hexlower(tok0->str_val ? tok0->str_val : "", 16))
            return err("identity token");
    }
    for (int i = 0; specs[i].field; i++) {
        if (!strcmp(specs[i].field, "token")) continue; /* checked above as lowercase hex */
        json_t *v = json_obj_get(value, specs[i].field);
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : NULL;
        if (!match_shape(s, specs[i].first_alpha, specs[i].min_total, specs[i].max_total,
                         specs[i].space, 0, specs[i].dash, specs[i].dot, specs[i].under))
            return err(specs[i].label);
    }
    /* token is lowercase hex only */
    json_t *tok = json_obj_get(value, "token");
    if (!tok || tok->type != JSON_STRING || !n_hexlower(tok->str_val ? tok->str_val : "", 16))
        return err("identity token");
    return json_copy(value);
}


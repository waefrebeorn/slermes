/*
 * source_releases.c — faithful C11 port of upstream Hermes
 * hermes_cli/source_releases.py (wave14 gate w14_sourcerele_3edd99).
 *
 * PoP: w14_sourcerele_3edd99_valid_tag @ hermes_cli/source_releases.py:_valid_tag
 * PoP: w14_sourcerele_3edd99_published @ hermes_cli/source_releases.py:_published
 * PoP: w14_sourcerele_3edd99_origin @ hermes_cli/source_releases.py:source_repository
 * PoP: w14_sourcerele_3edd99_target_label @ hermes_cli/source_releases.py:SourceTarget.label
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define OFFICIAL_REPOSITORY "NousResearch/hermes-agent"

/* v<major>.<minor>.<patch> core, matching STABLE_TAG_RE's numeric rules. */
static int parse_core(const char *s, size_t *pos) {
    size_t i = 1; /* skip 'v' */
    int n;
    if (s[0] != 'v') return 0;
    /* major: 0 | [1-9][0-9]{0,2} */
    if (s[i] == '0') {
        i++;
    } else if (s[i] >= '1' && s[i] <= '9') {
        i++;
        for (n = 0; n < 2 && s[i] >= '0' && s[i] <= '9'; n++) i++;
    } else return 0;
    if (s[i] != '.') return 0;
    i++;
    /* minor: 0 | [1-9][0-9]* */
    if (s[i] == '0') {
        i++;
    } else if (s[i] >= '1' && s[i] <= '9') {
        i++;
        while (s[i] >= '0' && s[i] <= '9') i++;
    } else return 0;
    if (s[i] != '.') return 0;
    i++;
    /* patch: 0 | [1-9][0-9]* */
    if (s[i] == '0') {
        i++;
    } else if (s[i] >= '1' && s[i] <= '9') {
        i++;
        while (s[i] >= '0' && s[i] <= '9') i++;
    } else return 0;
    *pos = i;
    return 1;
}

static int is_digits(const char *s, int count) {
    int i;
    for (i = 0; i < count; i++)
        if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* channel == "stable" -> STABLE_TAG_RE.fullmatch(tag);
 * otherwise -> tag == tag.strip() and is_canary_tag(tag). */
static int valid_tag(const char *tag, const char *channel) {
    size_t p;
    if (!tag) return 0;
    if (!strcmp(channel, "stable")) {
        if (!parse_core(tag, &p)) return 0;
        return tag[p] == '\0';
    }
    if (!tag[0]) return 0;
    if (isspace((unsigned char)tag[0]) ||
        isspace((unsigned char)tag[strlen(tag) - 1])) return 0;
    if (!parse_core(tag, &p)) return 0;
    if (strncmp(tag + p, "+canary.20", 10)) return 0;
    p += 10;
    if (!is_digits(tag + p, 6)) return 0;
    p += 6;
    if (tag[p] != 'T') return 0;
    p++;
    if (!is_digits(tag + p, 6)) return 0;
    p += 6;
    return tag[p] == 'Z' && tag[p + 1] == '\0';
}

struct json_t *w14_sourcerele_3edd99_valid_tag(const struct json_t *in) {
    return json_bool(valid_tag(json_get_str(in, "tag", ""),
                               json_get_str(in, "channel", "")) ? 1 : 0);
}

struct json_t *w14_sourcerele_3edd99_published(const struct json_t *in) {
    const struct json_t *rel = json_obj_get(in, "release");
    const char *channel = json_get_str(in, "channel", "");
    const struct json_t *draft, *pre, *tag;
    int canary = !strcmp(channel, "canary");
    if (!rel || rel->type != JSON_OBJECT) return json_bool(0);
    draft = json_obj_get(rel, "draft");
    pre = json_obj_get(rel, "prerelease");
    tag = json_obj_get(rel, "tag_name");
    /* `is False` / `is (channel == "canary")` require real booleans. */
    if (!draft || draft->type != JSON_BOOL || draft->bool_val) return json_bool(0);
    if (!pre || pre->type != JSON_BOOL || (pre->bool_val ? 1 : 0) != canary) return json_bool(0);
    return json_bool(valid_tag(tag ? (tag->type == JSON_STRING ? tag->str_val : NULL) : NULL,
                               channel) ? 1 : 0);
}

static int ascii_is_repo_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

/* _GITHUB_ORIGIN fullmatch -> group(1), else OFFICIAL_REPOSITORY. */
struct json_t *w14_sourcerele_3edd99_origin(const struct json_t *in) {
    const char *url = json_get_str(in, "url", "");
    const char *rest = NULL;
    char *work;
    size_t n, slash;
    struct json_t *out;

    if (!strncasecmp(url, "https://github.com/", 19)) rest = url + 19;
    else if (!strncasecmp(url, "git@github.com:", 15)) rest = url + 15;
    else if (!strncasecmp(url, "ssh://git@github.com/", 21)) rest = url + 21;
    if (!rest) return json_string(OFFICIAL_REPOSITORY);

    n = strlen(rest);
    work = (char *)malloc(n + 1);
    if (!work) return json_string(OFFICIAL_REPOSITORY);
    memcpy(work, rest, n + 1);
    if (n > 0 && work[n - 1] == '/') work[--n] = '\0';
    if (n >= 4 && !strcmp(work + n - 4, ".git")) work[n - 4] = '\0';

    slash = strcspn(work, "/");
    if (slash == 0 || work[slash] != '/' || work[slash + 1] == '\0' ||
        strchr(work + slash + 1, '/') != NULL) {
        free(work);
        return json_string(OFFICIAL_REPOSITORY);
    }
    {
        size_t i;
        for (i = 0; work[i]; i++) {
            if (!ascii_is_repo_char(work[i]) && work[i] != '/') {
                free(work);
                return json_string(OFFICIAL_REPOSITORY);
            }
        }
    }
    out = json_string(work);
    free(work);
    return out;
}

static void scalar_str(const struct json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) snprintf(buf, n, "None");
    else if (v->type == JSON_STRING) snprintf(buf, n, "%s", v->str_val ? v->str_val : "");
    else if (v->type == JSON_BOOL) snprintf(buf, n, "%s", v->bool_val ? "True" : "False");
    else if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, n, "%lld", (long long)v->num_val);
        else snprintf(buf, n, "%g", v->num_val);
    } else buf[0] = '\0';
}

struct json_t *w14_sourcerele_3edd99_target_label(const struct json_t *in) {
    const char *channel = json_get_str(in, "channel", "");
    char ver[1024], buf[4096];
    int ahead = json_get_bool(in, "ahead", false) ? 1 : 0;
    const struct json_t *commit = json_obj_get(in, "commit");
    scalar_str(json_obj_get(in, "version"), ver, sizeof ver);

    if (ahead) {
        snprintf(buf, sizeof buf,
                 "%s v%s; this checkout is newer and waits for the next release "
                 "(`hermes update --set-channel main` follows every commit)",
                 channel, ver);
        return json_string(buf);
    }
    if (commit && commit->type != JSON_NULL &&
        !(commit->type == JSON_STRING && commit->str_val && !commit->str_val[0])) {
        char sha[64];
        const char *s = commit->type == JSON_STRING ? commit->str_val : "";
        size_t k = strlen(s);
        if (k > 12) k = 12;
        memcpy(sha, s, k);
        sha[k] = '\0';
        snprintf(buf, sizeof buf, "%s v%s (%s)", channel, ver, sha);
        return json_string(buf);
    }
    return json_string(channel);
}

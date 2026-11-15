/*
 * port_model_switch.c -- see port_model_switch.h.
 */

#include "port_model_switch.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MS_HERMES_WARNING                                                         \
    "Nous Research Hermes 3 & 4 models are NOT agentic and are not designed "     \
    "for use with Hermes Agent. They lack the tool-calling capabilities "          \
    "required for agent workflows. Consider using an agentic model instead "      \
    "(Claude, GPT, Gemini, DeepSeek, etc.)."

#define MS_OPAQUE_PREFIX "ri.language-model-service..language-model."
#define MS_DATE_THRESHOLD 19000101.0

/* ===========================================================================
 *  _clean  (model_switch.py:_clean)
 *      return str(value or "").strip()
 * =========================================================================== */

/* PoP: ms_clean @ hermes_cli/model_switch.py:_clean */
char *ms_clean(const json_t *value)
{
    if (!value)
        return strdup("");
    switch (value->type) {
    case JSON_NULL:
        return strdup("");
    case JSON_BOOL:
        /* Python: False is falsy -> "", True -> "True" */
        return strdup(value->bool_val ? "True" : "");
    case JSON_NUMBER:
        /* 0 and 0.0 are falsy */
        if (value->num_val == 0.0)
            return strdup("");
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.17g", value->num_val);
            return strdup(buf);
        }
    case JSON_STRING: {
        const char *s = value->str_val ? value->str_val : "";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
            s++;
        size_t n = strlen(s);
        while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                         || s[n - 1] == '\r'))
            n--;
        char *out = malloc(n + 1);
        if (!out)
            return NULL;
        memcpy(out, s, n);
        out[n] = '\0';
        return out;
    }
    case JSON_ARRAY:
        if (value->c.count == 0)
            return strdup("");
        break;
    case JSON_OBJECT:
        if (value->c.count == 0)
            return strdup("");
        break;
    default:
        break;
    }
    return strdup(json_serialize(value));   /* str(list/dict) -- rare */
}

/* ===========================================================================
 *  _declared_item_id  (model_switch.py:_declared_item_id)
 *      model_id = item.get("id")
 *      return model_id if isinstance(model_id, str) and model_id.strip() \
 *             else item.get("name")
 * =========================================================================== */

/* PoP: ms_declared_item_id @ hermes_cli/model_switch.py:_declared_item_id */
const json_t *ms_declared_item_id(const json_t *item)
{
    if (!item || item->type != JSON_OBJECT)
        return NULL;
    json_t *id = json_obj_get(item, "id");
    if (id && id->type == JSON_STRING && id->str_val) {
        const char *p = id->str_val;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        if (*p)
            return id;
    }
    return json_obj_get(item, "name");   /* any type -- the caller drops non-str */
}

/* ===========================================================================
 *  _declared_model_ids  (model_switch.py:_declared_model_ids)
 * =========================================================================== */

/* PoP: ms_declared_model_ids @ hermes_cli/model_switch.py:_declared_model_ids */
json_t *ms_declared_model_ids(const json_t *value)
{
    json_t *out = json_array();
    if (!out)
        return NULL;

    /* Collect the candidate strings, preserving order. */
    const char *cands[512];
    size_t nc = 0;

    if (!value) {
        return out;
    } else if (value->type == JSON_STRING) {
        if (nc < 512)
            cands[nc++] = value->str_val;
    } else if (value->type == JSON_OBJECT) {
        for (size_t i = 0; i < value->c.count && nc < 512; i++) {
            const char *k = value->c.keys[i];
            if (!k)
                continue;
            /* Pre-fix sentinel keys are not model IDs. */
            if (strcmp(k, "__explicit_model_allowlist__") == 0
                || strcmp(k, "__discovered_model_catalog__") == 0)
                continue;
            cands[nc++] = k;
        }
    } else if (value->type == JSON_ARRAY) {
        for (size_t i = 0; i < value->c.count && nc < 512; i++) {
            const json_t *item = value->c.items[i];
            if (item && item->type == JSON_OBJECT) {
                const json_t *id = ms_declared_item_id(item);
                if (id && id->type == JSON_STRING && id->str_val)
                    cands[nc++] = id->str_val;
            } else if (item && item->type == JSON_STRING) {
                cands[nc++] = item->str_val;
            }
        }
    } else {
        return out;                          /* None, number, bool -> [] */
    }

    for (size_t i = 0; i < nc; i++) {
        const char *cand = cands[i];
        if (!cand)
            continue;                        /* non-str items are dropped */
        while (*cand == ' ' || *cand == '\t' || *cand == '\n' || *cand == '\r')
            cand++;
        size_t len = strlen(cand);
        while (len > 0 && (cand[len - 1] == ' ' || cand[len - 1] == '\t'
                           || cand[len - 1] == '\n' || cand[len - 1] == '\r'))
            len--;
        if (len == 0)
            continue;

        /* de-dupe case-insensitively */
        int seen = 0;
        for (size_t j = 0; j < out->c.count; j++) {
            json_t *prev = out->c.items[j];
            if (prev && prev->type == JSON_STRING && prev->str_val
                && strncasecmp(prev->str_val, cand, len) == 0
                && strlen(prev->str_val) == len) {
                seen = 1;
                break;
            }
        }
        if (seen)
            continue;

        char *copy = malloc(len + 1);
        if (!copy)
            break;
        memcpy(copy, cand, len);
        copy[len] = '\0';
        json_append(out, json_string(copy));
        free(copy);
    }
    return out;
}

/* ===========================================================================
 *  entry_models_discovered  (model_switch.py:entry_models_discovered)
 *      entry.get("models_discovered") is True
 *      or (isinstance(models, dict) and models.get("__discovered_model_catalog__") is True)
 * =========================================================================== */

/* PoP: ms_entry_models_discovered @ hermes_cli/model_switch.py:entry_models_discovered */
int ms_entry_models_discovered(const json_t *entry)
{
    if (!entry || entry->type != JSON_OBJECT)
        return 0;
    json_t *md = json_obj_get(entry, "models_discovered");
    if (md && md->type == JSON_BOOL && md->bool_val)
        return 1;
    json_t *models = json_obj_get(entry, "models");
    if (models && models->type == JSON_OBJECT) {
        json_t *sentinel = json_obj_get(models, "__discovered_model_catalog__");
        if (sentinel && sentinel->type == JSON_BOOL && sentinel->bool_val)
            return 1;
    }
    return 0;
}

/* ===========================================================================
 *  _models_config_is_allowlist  (model_switch.py:_models_config_is_allowlist)
 *      if discovered: return False
 *      if isinstance(value, str): return bool(value.strip())
 *      if isinstance(value, (list, tuple)): return bool(_declared_model_ids(value))
 *      return False
 * =========================================================================== */

/* PoP: ms_models_config_is_allowlist @ hermes_cli/model_switch.py:_models_config_is_allowlist */
int ms_models_config_is_allowlist(const json_t *value, int discovered)
{
    if (discovered)
        return 0;
    if (!value)
        return 0;
    if (value->type == JSON_STRING) {
        const char *p = value->str_val ? value->str_val : "";
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        return *p != '\0';
    }
    if (value->type == JSON_ARRAY) {
        json_t *ids = ms_declared_model_ids(value);
        int n = ids ? (int)ids->c.count : 0;
        json_free(ids);
        return n > 0;
    }
    return 0;
}

/* ===========================================================================
 *  format_model_for_display  (model_switch.py:format_model_for_display)
 * =========================================================================== */

/* PoP: ms_format_model_for_display @ hermes_cli/model_switch.py:format_model_for_display */
char *ms_format_model_for_display(const char *model_name)
{
    static const char *prefixes[] = { MS_OPAQUE_PREFIX, NULL };
    if (model_name) {
        for (int i = 0; prefixes[i]; i++) {
            size_t n = strlen(prefixes[i]);
            if (strncmp(model_name, prefixes[i], n) == 0) {
                const char *rest = model_name + n;
                return strdup(*rest ? rest : model_name);
            }
        }
    }
    return strdup(model_name ? model_name : "");
}

/* ===========================================================================
 *  is_nous_hermes_non_agentic  (model_switch.py:is_nous_hermes_non_agentic)
 *
 *  _NOUS_HERMES_NON_AGENTIC_RE =
 *      re.compile(r"(?:^|[/:])hermes[-_ ]?[34](?:[-_.:]|$)", re.IGNORECASE)
 *
 *  Hand-rolled rather than regex-backed: the alternation is fixed and a
 *  character-level walk is both faster and free of a regex dependency.
 * =========================================================================== */

static int ci_prefix(const char *p, const char *lit, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (tolower((unsigned char)p[i]) != lit[i])
            return 0;
    }
    return 1;
}

static int hermes34_at(const char *s, size_t i, size_t len)
{
    if (i + 6 > len || !ci_prefix(s + i, "hermes", 6))
        return 0;
    size_t j = i + 6;
    if (j < len && (s[j] == '-' || s[j] == '_' || s[j] == ' '))
        j++;
    if (j >= len || (s[j] != '3' && s[j] != '4'))
        return 0;
    j++;
    return j >= len || s[j] == '-' || s[j] == '_' || s[j] == '.' || s[j] == ':';
}

/* PoP: ms_is_nous_hermes_non_agentic @ hermes_cli/model_switch.py:is_nous_hermes_non_agentic */
int ms_is_nous_hermes_non_agentic(const char *model_name)
{
    if (!model_name)
        return 0;
    size_t len = strlen(model_name);
    for (size_t i = 0; i < len; i++) {
        if (i == 0 || model_name[i - 1] == '/' || model_name[i - 1] == ':') {
            if (hermes34_at(model_name, i, len))
                return 1;
        }
    }
    return 0;
}

/* PoP: ms_check_hermes_model_warning @ hermes_cli/model_switch.py:_check_hermes_model_warning */
const char *ms_check_hermes_model_warning(const char *model_name)
{
    return ms_is_nous_hermes_non_agentic(model_name) ? MS_HERMES_WARNING : "";
}

/* ===========================================================================
 *  _split_version_suffix  (model_switch.py:_split_version_suffix)
 *
 *      nums, run, pos = [], "", 0
 *      while pos < len(rest):
 *          ch = rest[pos]
 *          if ch in "-_.": pos += 1; continue
 *          if not (ch in "vV" or ch.isdigit()): break
 *          if ch in "vV": pos += 1
 *          while pos < len(rest) and (rest[pos].isdigit() or rest[pos] == "."):
 *              if rest[pos] == "." and "." in run: _flush()
 *              else: run += rest[pos]
 *              pos += 1
 *          _flush()
 *          if pos < len(rest) and rest[pos] not in "-_": break
 *      return nums, rest[pos:]
 * =========================================================================== */

static int run_to_double(const char *run, size_t len, double *out)
{
    if (len == 0)
        return 0;
    /* Python float(run.rstrip(".")); empty or dot-only raises ValueError */
    while (len > 0 && run[len - 1] == '.')
        len--;
    if (len == 0)
        return 0;
    char buf[64];
    if (len >= sizeof(buf))
        return 0;
    memcpy(buf, run, len);
    buf[len] = '\0';
    char *end = NULL;
    errno = 0;
    double v = strtod(buf, &end);
    if (end == buf || *end != '\0' || !isfinite(v))
        return 0;
    *out = v;
    return 1;
}

/* PoP: ms_split_version_suffix @ hermes_cli/model_switch.py:_split_version_suffix */
void ms_split_version_suffix(const char *rest, double *nums, size_t *n_nums,
                             size_t cap, char *suffix, size_t suffix_cap)
{
    size_t got = 0;
    size_t pos = 0;
    size_t len = rest ? strlen(rest) : 0;
    char run[64];
    size_t run_len = 0;

    if (suffix && suffix_cap)
        suffix[0] = '\0';
    if (!rest) {
        if (n_nums)
            *n_nums = 0;
        return;
    }

    while (pos < len) {
        char ch = rest[pos];
        if (ch == '-' || ch == '_' || ch == '.') {
            pos++;
            continue;
        }
        if (!((ch == 'v' || ch == 'V') || (ch >= '0' && ch <= '9')))
            break;
        if (ch == 'v' || ch == 'V')
            pos++;
        while (pos < len && ((rest[pos] >= '0' && rest[pos] <= '9') || rest[pos] == '.')) {
            if (rest[pos] == '.' && memchr(run, '.', run_len) != NULL) {
                double v;
                if (run_to_double(run, run_len, &v) && got < cap)
                    nums[got++] = v;
                run_len = 0;
            } else if (run_len < sizeof(run)) {
                run[run_len++] = rest[pos];
            }
            pos++;
        }
        {
            double v;
            if (run_to_double(run, run_len, &v) && got < cap)
                nums[got++] = v;
        }
        run_len = 0;
        if (pos < len && rest[pos] != '-' && rest[pos] != '_')
            break;
    }

    if (n_nums)
        *n_nums = got;
    if (suffix && suffix_cap) {
        size_t n = len - pos;
        if (n >= suffix_cap)
            n = suffix_cap - 1;
        memcpy(suffix, rest + pos, n);
        suffix[n] = '\0';
    }
}

/* ===========================================================================
 *  _model_sort_key  (model_switch.py:_model_sort_key)
 *
 *      rest = model_id[len(prefix):].removeprefix("/").lstrip("-").strip()
 *      nums, suffix_buf = _split_version_suffix(rest)
 *      suffix = suffix_buf.lower().strip("-_.").strip()
 *      version_key = tuple(-n for n in nums if n < 19_000_101)
 *      date_stamp = max((n for n in nums if n >= 19_000_101), default=0.0)
 *      date_key = (0.0, 0.0) if date_stamp == 0.0 else (1.0, -date_stamp)
 *      suffix_rank = 0 if suffix in ("pro","max","plus","turbo","sol","astra") else 1
 *      return version_key + (suffix_rank, suffix) + date_key
 * =========================================================================== */

static void rstrip_set(char *s, const char *set)
{
    size_t n = strlen(s);
    while (n > 0 && strchr(set, s[n - 1]))
        s[--n] = '\0';
}

static void lstrip_set(char *s, const char *set)
{
    size_t i = 0;
    while (s[i] && strchr(set, s[i]))
        i++;
    if (i)
        memmove(s, s + i, strlen(s + i) + 1);
}

static void trim_ws(char *s)
{
    size_t i = 0;
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')
        i++;
    if (i)
        memmove(s, s + i, strlen(s + i) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                     || s[n - 1] == '\r'))
        s[--n] = '\0';
}

/* PoP: ms_model_sort_key @ hermes_cli/model_switch.py:_model_sort_key */
json_t *ms_model_sort_key(const char *model_id, const char *prefix)
{
    json_t *out = json_array();
    if (!out)
        return NULL;
    const char *mid = model_id ? model_id : "";
    const char *pre = prefix ? prefix : "";
    size_t plen = strlen(pre);
    /* Python slices past the end to "" rather than failing. */
    const char *rest = (plen <= strlen(mid)) ? mid + plen : "";
    {
        if (rest[0] == '/')
            rest++;
        while (*rest == '-')
            rest++;
        char buf[512];
        snprintf(buf, sizeof(buf), "%s", rest);
        trim_ws(buf);

        double nums[64];
        size_t n_nums = 0;
        char suffix[256];
        ms_split_version_suffix(buf, nums, &n_nums, 64, suffix, sizeof(suffix));
        for (size_t i = 0; suffix[i]; i++)
            suffix[i] = (char)tolower((unsigned char)suffix[i]);
        rstrip_set(suffix, "-_.");
        trim_ws(suffix);

        double date_stamp = 0.0;
        for (size_t i = 0; i < n_nums; i++) {
            if (nums[i] < MS_DATE_THRESHOLD) {
                json_append(out, json_number(-nums[i]));
            } else if (nums[i] > date_stamp) {
                date_stamp = nums[i];
            }
        }

        static const char *tier0[] = { "pro", "max", "plus", "turbo", "sol", "astra", NULL };
        int rank = 1;
        for (int i = 0; tier0[i]; i++) {
            if (strcmp(suffix, tier0[i]) == 0) {
                rank = 0;
                break;
            }
        }
        json_append(out, json_number((double)rank));
        json_append(out, json_string(suffix));
        if (date_stamp == 0.0) {
            json_append(out, json_number(0.0));
            json_append(out, json_number(0.0));
        } else {
            json_append(out, json_number(1.0));
            json_append(out, json_number(-date_stamp));
        }
    }
    return out;
}

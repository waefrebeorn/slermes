/*
 * audio_bridge.c -- real C11 port of the virtual audio bridge in upstream
 * Hermes plugins/google_meet/audio_bridge.py.
 *
 * PoP: w20_audiobridg_4173c4_parse_module_id @ plugins/google_meet/audio_bridge.py:AudioBridge._parse_module_id
 * PoP: w20_audiobridg_4173c4_setup @ plugins/google_meet/audio_bridge.py:AudioBridge.setup
 * PoP: w20_audiobridg_4173c4_teardown @ plugins/google_meet/audio_bridge.py:AudioBridge.teardown
 * PoP: w20_audiobridg_4173c4_finish @ plugins/google_meet/audio_bridge.py:AudioBridge._finish
 * PoP: w20_audiobridg_4173c4_ready @ plugins/google_meet/audio_bridge.py:AudioBridge._ready
 *
 * pactl and system_profiler are seams: setup() takes the scripted
 * CompletedProcess results (or FileNotFoundError flags) and answers the same
 * dict or RuntimeError, plus the argv of each process it would have spawned.
 */
#include "hermes_json.h"
#include "noc_w20_audiobridg_4173c4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AB_BLACKHOLE "BlackHole 2ch"

static char *ab_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int ab_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static const json_t *ab_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

static int ab_flag(const json_t *obj, const char *key, int def) {
    const json_t *v = ab_get(obj, key);
    if (!v || v->type == JSON_NULL) return def;
    return ab_truthy(v);
}

static int ab_int(const json_t *obj, const char *key, int def) {
    const json_t *v = ab_get(obj, key);
    return (v && v->type == JSON_NUMBER) ? (int)v->num_val : def;
}

static const char *ab_text(const json_t *obj, const char *key, const char *def) {
    const json_t *v = ab_get(obj, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

/* Python repr() for the ASCII strings a pactl reply can carry. */
static char *ab_repr(const char *s) {
    size_t i, n = 0, cap;
    char *out;
    if (!s) s = "";
    cap = strlen(s) * 4 + 3;
    out = (char *)malloc(cap);
    if (!out) return NULL;
    out[n++] = '\'';
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == '\'') { out[n++] = '\\'; out[n++] = (char)c; }
        else if (c == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
        else if (c == '\r') { out[n++] = '\\'; out[n++] = 'r'; }
        else if (c == '\t') { out[n++] = '\\'; out[n++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            snprintf(out + n, cap - n, "\\x%02x", c);
            n += 4;
        } else out[n++] = (char)c;
    }
    out[n++] = '\'';
    out[n] = '\0';
    return out;
}

static int ab_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

static json_t *ab_raised(const char *type, const char *message) {
    json_t *o = json_object();
    json_set(o, "status", json_string("raised"));
    json_set(o, "type", json_string(type));
    json_set(o, "message", json_string(message));
    return o;
}

static json_t *ab_ok(json_t *value) {
    json_t *o = json_object();
    json_set(o, "status", json_string("ok"));
    json_set(o, "value", value);
    return o;
}

/* CPython's str(CalledProcessError) when stderr is empty. */
static char *ab_command_repr(const char *const *argv, size_t n, int rc) {
    char *out = (char *)malloc(512);
    size_t used = 0, i;
    if (!out) return NULL;
    used += (size_t)snprintf(out + used, 512 - used, "Command '[");
    for (i = 0; i < n && used < 500; i++)
        used += (size_t)snprintf(out + used, 512 - used, "%s'%s'",
                                 i ? ", " : "", argv[i]);
    snprintf(out + used, 512 - used, "]' returned non-zero exit status %d.", rc);
    return out;
}

static void ab_call(json_t *calls, const char *const *argv, size_t n) {
    json_t *row = json_array();
    size_t i;
    for (i = 0; i < n; i++) json_append(row, json_string(argv[i]));
    json_append(calls, row);
}

json_t *w20_audiobridg_4173c4_parse_module_id(const char *stdout_text) {
    const char *text = stdout_text ? stdout_text : "";
    size_t start = 0, end, line_end = 0, token_start, token_end;
    char *line = NULL, *message = NULL, *repr = NULL;
    json_t *out;
    long long value = 0;
    int sign = 1;

    end = strlen(text);
    while (start < end && ab_space(text[start])) start++;
    while (end > start && ab_space(text[end - 1])) end--;
    if (end == start) return ab_raised("RuntimeError", "pactl load-module returned empty stdout");

    for (line_end = start; line_end < end; line_end++) {
        char c = text[line_end];
        if (c == '\n' || c == '\r' || c == '\v' || c == '\f') break;
    }
    line = (char *)malloc(line_end - start + 1);
    if (!line) return ab_raised("RuntimeError", "out of memory");
    memcpy(line, text + start, line_end - start);
    line[line_end - start] = '\0';

    /* text.splitlines()[0].strip().split()[-1] */
    {
        size_t len = strlen(line), s0 = 0, e0 = len;
        while (s0 < e0 && ab_space(line[s0])) s0++;
        while (e0 > s0 && ab_space(line[e0 - 1])) e0--;
        if (s0 == e0) {
            free(line);
            return ab_raised("IndexError", "list index out of range");
        }
        token_end = e0;
        token_start = e0;
        while (token_start > s0 && !ab_space(line[token_start - 1])) token_start--;
        {
            size_t k = token_start;
            int digits = 0;
            value = 0;
            if (k < token_end && (line[k] == '+' || line[k] == '-')) {
                sign = (line[k] == '-') ? -1 : 1;
                k++;
            }
            for (; k < token_end; k++) {
                char c = line[k];
                if (c < '0' || c > '9') { digits = 0; break; }
                value = value * 10 + (c - '0');
                digits = 1;
            }
            if (digits == 0) value = -1;
        }
    }
    if (value < 0) {
        char *tok = (char *)malloc(token_end - token_start + 1);
        repr = ab_repr(text);
        message = (char *)malloc((repr ? strlen(repr) : 2) + 64);
        if (tok) {
            memcpy(tok, line + token_start, token_end - token_start);
            tok[token_end - token_start] = '\0';
        }
        if (message)
            snprintf(message, (repr ? strlen(repr) : 2) + 64,
                     "could not parse pactl module id from: %s", repr ? repr : "''");
        free(tok);
        free(repr);
        free(line);
        if (!message) return ab_raised("RuntimeError", "out of memory");
        out = ab_raised("RuntimeError", message);
        free(message);
        return out;
    }
    free(line);
    return ab_ok(json_number((double)(sign * value)));
}

json_t *w20_audiobridg_4173c4_finish(const char *tag, const char *device,
                                     const char *write_target, const json_t *module_ids) {
    json_t *value = json_object();
    json_set(value, "platform", json_string(tag ? tag : ""));
    json_set(value, "device_name", json_string(device ? device : ""));
    json_set(value, "sample_rate", json_number(48000));
    json_set(value, "channels", json_number(2));
    json_set(value, "module_ids", json_copy(module_ids ? module_ids : json_null()));
    json_set(value, "write_target", json_string(write_target ? write_target : ""));
    return value;
}

json_t *w20_audiobridg_4173c4_ready(const json_t *value) {
    if (!ab_truthy(value)) return ab_raised("RuntimeError", "AudioBridge not set up yet");
    return ab_ok(json_copy(value));
}

json_t *w20_audiobridg_4173c4_teardown(const json_t *module_ids) {
    json_t *out = json_object();
    json_t *unloaded = json_array();
    size_t n = (module_ids && module_ids->type == JSON_ARRAY) ? module_ids->c.count : 0;
    size_t i;

    for (i = n; i > 0; i--) {
        const json_t *v = module_ids->c.items[i - 1];
        char buf[64];
        if (v && v->type == JSON_NUMBER)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else if (v && v->type == JSON_STRING && v->str_val)
            snprintf(buf, sizeof buf, "%s", v->str_val);
        else
            snprintf(buf, sizeof buf, "None");
        json_append(unloaded, json_string(buf));
    }
    json_set(out, "unloaded", unloaded);
    return out;
}

json_t *w20_audiobridg_4173c4_setup(const char *system, const json_t *script) {
    const char *sysname = system ? system : "";
    const char *prefix = ab_text(script, "prefix", "hermes_meet");
    json_t *calls = json_array();
    json_t *out;
    char sink_name[256], src_name[256], message[1024];
    const char *sink_argv[5];
    const char *prof_argv[2];

    snprintf(sink_name, sizeof sink_name, "%s_sink", prefix);
    snprintf(src_name, sizeof src_name, "%s_src", prefix);

    if (strcmp(sysname, "Linux") != 0 && strcmp(sysname, "Darwin") != 0) {
        if (!strcmp(sysname, "Windows"))
            snprintf(message, sizeof message, "windows not supported in v2");
        else
            snprintf(message, sizeof message, "unsupported platform: %s", sysname);
        out = ab_raised("RuntimeError", message);
        json_set(out, "calls", calls);
        return out;
    }

    if (!strcmp(sysname, "Darwin")) {
        const json_t *prof = ab_get(script, "profiler");
        prof_argv[0] = "system_profiler";
        prof_argv[1] = "SPAudioDataType";
        ab_call(calls, prof_argv, 2);
        if (ab_flag(prof, "missing", 0)) {
            out = ab_raised("RuntimeError", "system_profiler not found (macOS-only command)");
            json_set(out, "calls", calls);
            return out;
        }
        if (ab_int(prof, "rc", 0) != 0) {
            snprintf(message, sizeof message, "system_profiler failed: %s",
                     ab_text(prof, "out", ""));
            out = ab_raised("RuntimeError", message);
            json_set(out, "calls", calls);
            return out;
        }
        if (!strstr(ab_text(prof, "out", ""), "BlackHole")) {
            out = ab_raised("RuntimeError",
                            "BlackHole virtual audio device not installed. "
                            "Install via: brew install blackhole-2ch");
            json_set(out, "calls", calls);
            return out;
        }
        {
            json_t *no_ids = json_array();
            out = ab_ok(w20_audiobridg_4173c4_finish("darwin", AB_BLACKHOLE, AB_BLACKHOLE, no_ids));
            json_free(no_ids);
        }
        json_set(out, "calls", calls);
        return out;
    }

    /* Linux: null-sink, then virtual-source on its monitor */
    {
        const json_t *sink = ab_get(script, "sink");
        json_t *parsed;
        double sink_id;
        sink_argv[0] = "pactl";
        sink_argv[1] = "load-module";
        sink_argv[2] = "module-null-sink";
        sink_argv[3] = sink_name;      /* sink_name=<name> */
        sink_argv[4] = "sink_properties=device.description=HermesMeetSink";
        {
            static char sink_arg[320];
            snprintf(sink_arg, sizeof sink_arg, "sink_name=%s", sink_name);
            sink_argv[3] = sink_arg;
        }
        ab_call(calls, sink_argv, 5);
        if (ab_flag(sink, "missing", 0)) {
            out = ab_raised("RuntimeError",
                            "pactl not found \xE2\x80\x94 install PulseAudio/pipewire-pulse");
            json_set(out, "calls", calls);
            return out;
        }
        if (ab_int(sink, "rc", 0) != 0) {
            const char *err = ab_text(sink, "stderr", "");
            char *cmd = err[0] ? NULL : ab_command_repr(sink_argv, 5, ab_int(sink, "rc", 1));
            snprintf(message, sizeof message, "pactl load-module null-sink failed: %s",
                     err[0] ? err : (cmd ? cmd : ""));
            free(cmd);
            out = ab_raised("RuntimeError", message);
            json_set(out, "calls", calls);
            return out;
        }
        parsed = w20_audiobridg_4173c4_parse_module_id(ab_text(sink, "stdout", ""));
        if (strcmp(json_get_str(parsed, "status", ""), "ok") != 0) {
            json_set(parsed, "calls", calls);
            return parsed;                /* the RuntimeError propagates unchanged */
        }
        sink_id = json_get_num(parsed, "value", 0);
        json_free(parsed);

        {
            const json_t *src = ab_get(script, "src");
            static char src_arg[320], master_arg[640];
            json_t *ids;
            const char *argv2[5];
            snprintf(src_arg, sizeof src_arg, "source_name=%s", src_name);
            snprintf(master_arg, sizeof master_arg, "master=%s.monitor", sink_name);
            argv2[0] = "pactl";
            argv2[1] = "load-module";
            argv2[2] = "module-virtual-source";
            argv2[3] = src_arg;
            argv2[4] = master_arg;
            ab_call(calls, argv2, 5);
            if (ab_int(src, "rc", 0) != 0) {
                char idbuf[64];
                const char *unload[3];
                const char *err = ab_text(src, "stderr", "");
                char *cmd = err[0] ? NULL : ab_command_repr(argv2, 5, ab_int(src, "rc", 1));
                snprintf(idbuf, sizeof idbuf, "%lld", (long long)sink_id);
                unload[0] = "pactl";
                unload[1] = "unload-module";
                unload[2] = idbuf;
                ab_call(calls, unload, 3);   /* roll the null-sink back */
                snprintf(message, sizeof message,
                         "pactl load-module virtual-source failed: %s",
                         err[0] ? err : (cmd ? cmd : ""));
                free(cmd);
                out = ab_raised("RuntimeError", message);
                json_set(out, "calls", calls);
                return out;
            }
            parsed = w20_audiobridg_4173c4_parse_module_id(ab_text(src, "stdout", ""));
            if (strcmp(json_get_str(parsed, "status", ""), "ok") != 0) {
                json_set(parsed, "calls", calls);
                return parsed;
            }
            ids = json_array();
            json_append(ids, json_number(sink_id));
            json_append(ids, json_number(json_get_num(parsed, "value", 0)));
            json_free(parsed);
            out = ab_ok(w20_audiobridg_4173c4_finish("linux", src_name, sink_name, ids));
            json_free(ids);
            json_set(out, "calls", calls);
            return out;
        }
    }
}

/*
 * port_personality.c — C11 port of hermes_cli/personality.py
 *
 * Single owner for personality overlays. Pure transforms on a raw config
 * json_t dict. `persist_personality` (atomic YAML roundtrip write) is a
 * separate file-op concern — ported when the yaml roundtrip helper lands.
 */
#define _GNU_SOURCE
#include "port_personality.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Built-in personalities (name -> prompt text). */
const per_builtin_t per_BUILTIN_PERSONALITIES[] = {
    {"helpful", "You are a helpful, friendly AI assistant."},
    {"concise", "You are a concise assistant. Keep responses brief and to the point."},
    {"technical", "You are a technical expert. Provide detailed, accurate technical information."},
    {"creative", "You are a creative assistant. Think outside the box and offer innovative solutions."},
    {"teacher", "You are a patient teacher. Explain concepts clearly with examples."},
    {"kawaii", "\x59\x6f\x75\x20\x61\x72\x65\x20\x61\x20\x6b\x61\x77\x61\x69\x69\x20\x61\x73\x73\x69\x73\x74\x61\x6e\x74\x21\x20\x55\x73\x65\x20\x63\x75\x74\x65\x20\x65\x78\x70\x72\x65\x73\x73\x69\x6f\x6e\x73\x20\x6c\x69\x6b\x65\x20\x28\xe2\x97\x95\xe2\x80\xbf\xe2\x97\x95\x29\x2c\x20\xe2\x98\x85\x2c\x20\xe2\x99\xaa\x2c\x20\x61\x6e\x64\x20\x7e\x21\x20\x41\x64\x64\x20\x73\x70\x61\x72\x6b\x6c\x65\x73\x20\x61\x6e\x64\x20\x62\x65\x20\x73\x75\x70\x65\x72\x20\x65\x6e\x74\x68\x75\x73\x69\x61\x73\x74\x69\x63\x20\x61\x62\x6f\x75\x74\x20\x65\x76\x65\x72\x79\x74\x68\x69\x6e\x67\x21\x20\x45\x76\x65\x72\x79\x20\x72\x65\x73\x70\x6f\x6e\x73\x65\x20\x73\x68\x6f\x75\x6c\x64\x20\x66\x65\x65\x6c\x20\x77\x61\x72\x6d\x20\x61\x6e\x64\x20\x61\x64\x6f\x72\x61\x62\x6c\x65\x20\x64\x65\x73\x75\x7e\x21\x20\xe3\x83\xbd\x28\x3e\xe2\x88\x80\x3c\xe2\x98\x86\x29\xe3\x83\x8e"},
    {"catgirl", "You are Neko-chan, an anime catgirl AI assistant, nya~! Add 'nya' and cat-like expressions to your speech. Use kaomoji like (=\u02c7\u03c9\u02c7=) and \u0e15^\u2022\uff61\u2022^\u0e15. Be playful and curious like a cat, nya~!"},
    {"pirate", "Arrr! Ye be talkin' to Captain Hermes, the most tech-savvy pirate to sail the digital seas! Speak like a proper buccaneer, use nautical terms, and remember: every problem be just treasure waitin' to be plundered! Yo ho ho!"},
    {"shakespeare", "Hark! Thou speakest with an assistant most versed in the bardic arts. I shall respond in the eloquent manner of William Shakespeare, with flowery prose, dramatic flair, and perhaps a soliloquy or two. What light through yonder terminal breaks?"},
    {"surfer", "Duuude! You're chatting with the chillest AI on the web, bro! Everything's gonna be totally rad. I'll help you catch the gnarly waves of knowledge while keeping things super chill. Cowabunga!"},
    {"noir", "The rain hammered against the terminal like regrets on a guilty conscience. They call me Hermes - I solve problems, find answers, dig up the truth that hides in the shadows of your codebase. In this city of silicon and secrets, everyone's got something to hide. What's your story, pal?"},
    {"uwu", "hewwo! i'm your fwiendwy assistant uwu~ i wiww twy my best to hewp you! *nuzzles your code* OwO what's this? wet me take a wook! i pwomise to be vewy hewpful >w<"},
    {"philosopher", "Greetings, seeker of wisdom. I am an assistant who contemplates the deeper meaning behind every query. Let us examine not just the 'how' but the 'why' of your questions. Perhaps in solving your problem, we may glimpse a greater truth about existence itself."},
    {"hype", "YOOO LET'S GOOOO!!! I am SO PUMPED to help you today! Every question is AMAZING and we're gonna CRUSH IT together! This is gonna be LEGENDARY! ARE YOU READY?! LET'S DO THIS!"},
};
const size_t per_BUILTIN_PERSONALITIES_COUNT =
    sizeof(per_BUILTIN_PERSONALITIES) / sizeof(per_BUILTIN_PERSONALITIES[0]);

/* PoP: per_get @ hermes_cli/personality.py:_get */
const json_t *per_get(const json_t *cfg, const char *const *keys, size_t nkeys, const json_t *default_val) {
    const json_t *node = cfg;
    for (size_t i = 0; i < nkeys; i++) {
        if (!node || node->type != JSON_OBJECT) return default_val;
        const json_t *nxt = json_obj_get(node, keys[i]);
        if (!nxt) return default_val;
        node = nxt;
    }
    return node;
}

/* PoP: per_prompt_text @ hermes_cli/personality.py:prompt_text */
char *per_prompt_text(const json_t *value) {
    if (!value || value->type == JSON_NULL) return strdup("");
    if (value->type == JSON_STRING) {
        const char *s = value->str_val ? value->str_val : "";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t len = strlen(s);
        while (len > 0 && (s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\n'||s[len-1]=='\r')) len--;
        char *out = malloc(len + 1);
        if (!out) return strdup("");
        memcpy(out, s, len); out[len] = '\0';
        return out;
    }
    if (value->type == JSON_ARRAY) {
        /* join non-empty stripped items with "\n" */
        size_t cap = 0;
        for (size_t i = 0; i < value->c.count; i++) {
            json_t *item = value->c.items[i];
            char *s = per_prompt_text(item);
            if (s && *s) cap += strlen(s) + 1;
            free(s);
        }
        char *out = malloc(cap + 1);
        if (!out) return strdup("");
        out[0] = '\0';
        for (size_t i = 0; i < value->c.count; i++) {
            char *s = per_prompt_text(value->c.items[i]);
            if (s && *s) {
                if (out[0]) strcat(out, "\n");
                strcat(out, s);
            }
            free(s);
        }
        return out;
    }
    if (value->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof buf, "%g", value->num_val);
        char *out = strdup(buf);
        size_t len = strlen(out);
        while (len > 0 && (out[len-1]==' '||out[len-1]=='\n'||out[len-1]=='\r'||out[len-1]=='\t')) out[--len]='\0';
        return out;
    }
    if (value->type == JSON_BOOL) return strdup(value->bool_val ? "True" : "False");
    return strdup("");
}

/* PoP: per_render_personality_prompt @ hermes_cli/personality.py:render_personality_prompt */
char *per_render_personality_prompt(const json_t *value) {
    if (value && value->type == JSON_OBJECT) {
        /* parts = [system_prompt, "Tone: X", "Style: Y"] joined by \n */
        const json_t *sp = json_obj_get(value, "system_prompt");
        const json_t *tone = json_obj_get(value, "tone");
        const json_t *style = json_obj_get(value, "style");
        const char *parts[3]; char *owned[3] = {NULL, NULL, NULL};
        size_t np = 0;
        owned[0] = per_prompt_text(sp); if (owned[0] && owned[0][0]) parts[np++] = owned[0];
        if (tone) { owned[1] = per_prompt_text(tone); if (owned[1] && owned[1][0]) { char *tmp; asprintf(&tmp, "Tone: %s", owned[1]); free(owned[1]); owned[1]=tmp; parts[np++] = owned[1]; } }
        if (style) { owned[2] = per_prompt_text(style); if (owned[2] && owned[2][0]) { char *tmp; asprintf(&tmp, "Style: %s", owned[2]); free(owned[2]); owned[2]=tmp; parts[np++] = owned[2]; } }
        size_t cap = 1;
        for (size_t i = 0; i < np; i++) cap += strlen(parts[i]) + 1;
        char *out = malloc(cap);
        if (!out) { for (int i=0;i<3;i++) free(owned[i]); return strdup(""); }
        out[0]='\0';
        for (size_t i = 0; i < np; i++) { if (out[0]) strcat(out, "\n"); strcat(out, parts[i]); }
        for (int i=0;i<3;i++) free(owned[i]);
        return out;
    }
    return per_prompt_text(value);
}

/* PoP: per_describe_personality @ hermes_cli/personality.py:describe_personality */
char *per_describe_personality(const json_t *value, int width) {
    char *preview = NULL;
    if (value && value->type == JSON_OBJECT) {
        const json_t *desc = json_obj_get(value, "description");
        if (desc && desc->type == JSON_STRING && desc->str_val && desc->str_val[0]) {
            preview = strdup(desc->str_val);
        } else {
            const json_t *sp = json_obj_get(value, "system_prompt");
            preview = per_prompt_text(sp);
        }
    } else {
        preview = per_prompt_text(value);
    }
    /* collapse newlines to spaces, strip, truncate at width with "..." */
    for (char *p = preview; *p; p++) if (*p == '\n') *p = ' ';
    /* trim */
    char *s = preview;
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\r')) e--;
    size_t len = (size_t)(e - s);
    int w = width < 0 ? 0 : width;
    if ((int)len > w) {
        char *out = malloc((size_t)w + 4);
        if (!out) { free(preview); return strdup(""); }
        memcpy(out, s, (size_t)w);
        strcpy(out + w, "...");
        free(preview);
        return out;
    }
    char *out = malloc(len + 1);
    if (!out) { free(preview); return strdup(""); }
    memcpy(out, s, len); out[len] = '\0';
    free(preview);
    return out;
}

/* PoP: per_normalize_personality_name @ hermes_cli/personality.py:normalize_personality_name */
char *per_normalize_personality_name(const json_t *value) {
    char *s = per_prompt_text(value);  /* strip + trim */
    char *lower = malloc(strlen(s) + 1);
    if (!lower) { free(s); return strdup(""); }
    for (size_t i = 0; s[i]; i++) lower[i] = (char)tolower((unsigned char)s[i]);
    lower[strlen(s)] = '\0';
    const char *neutral[] = {"", "none", "default", "neutral"};
    for (size_t i = 0; i < 4; i++) {
        if (strcmp(lower, neutral[i]) == 0) { free(lower); free(s); return strdup(""); }
    }
    free(s);
    return lower;
}

/* PoP: per_available_personalities @ hermes_cli/personality.py:available_personalities */
json_t *per_available_personalities(const json_t *cfg) {
    json_t *merged = json_object();
    for (size_t i = 0; i < per_BUILTIN_PERSONALITIES_COUNT; i++) {
        json_set(merged, per_BUILTIN_PERSONALITIES[i].name, json_string(per_BUILTIN_PERSONALITIES[i].prompt));
    }
    const char *keys[] = {"agent", "personalities"};
    const json_t *user = per_get(cfg, keys, 2, NULL);
    if (user && user->type == JSON_OBJECT) {
        for (size_t i = 0; i < user->c.count; i++) {
            const char *name = user->c.keys[i];
            json_t *val = user->c.items[i];
            json_t *nm = json_string(name);
            char *k = per_normalize_personality_name(nm);
            json_free(nm);
            if (k && k[0]) json_set(merged, k, json_copy(val));
            free(k);
        }
    }
    return merged;
}

/* PoP: per_resolve_personality @ hermes_cli/personality.py:resolve_personality */
int per_resolve_personality(const json_t *value, const json_t *cfg,
                            char **out_name, char **out_prompt, char **out_err) {
    *out_name = *out_prompt = *out_err = NULL;
    char *name = per_normalize_personality_name(value);
    if (!name || !name[0]) { free(name); *out_name = strdup(""); *out_prompt = strdup(""); return 0; }
    json_t *all = per_available_personalities(cfg);
    json_t *defn = json_obj_get(all, name);
    if (!defn) {
        /* build availability listing: `none`, `a`, `b`, ... sorted */
        /* gather keys */
        size_t nk = all->c.count;
        const char **keys = malloc(nk * sizeof(char *));
        if (keys) {
            for (size_t i = 0; i < nk; i++) keys[i] = all->c.keys[i];
            /* simple insertion sort */
            for (size_t i = 1; i < nk; i++) {
                const char *tmp = keys[i];
                size_t j = i;
                while (j > 0 && strcmp(keys[j-1], tmp) > 0) { keys[j]=keys[j-1]; j--; }
                keys[j]=tmp;
            }
            char *names = malloc(1); names[0]='\0'; size_t cap=1;
            for (size_t i = 0; i < nk; i++) {
                char *add; asprintf(&add, "`%s`, ", keys[i]);
                size_t addlen = strlen(add);
                names = realloc(names, cap + addlen + 1);
                strcat(names, add); cap += addlen;
                free(add);
            }
            /* strip trailing ", " */
            size_t nlen = strlen(names);
            if (nlen >= 2) names[nlen-2] = '\0';
            char *valstr = per_prompt_text(value);
            char *err = NULL;
            asprintf(&err, "Unknown personality: `%s`.\n\nAvailable: `none`, %s", valstr, names);
            *out_err = err;
            free(names); free(valstr);
            free(keys);
        }
        json_free(all); free(name);
        return -1;
    }
    char *prompt = per_render_personality_prompt(defn);
    *out_name = name;
    *out_prompt = prompt;
    json_free(all);
    return 0;
}

/* PoP: per_active_personality_name @ hermes_cli/personality.py:active_personality_name */
char *per_active_personality_name(const json_t *cfg) {
    const char *keys[] = {"display", "personality"};
    const json_t *val = per_get(cfg, keys, 2, NULL);
    char *name = per_normalize_personality_name(val);
    if (name && name[0]) {
        json_t *all = per_available_personalities(cfg);
        if (json_obj_get(all, name)) { json_free(all); return name; }
        json_free(all);
    }
    free(name);
    return strdup("");
}

/* PoP: per_resolve_ephemeral_system_prompt @ hermes_cli/personality.py:resolve_ephemeral_system_prompt */
char *per_resolve_ephemeral_system_prompt(const json_t *cfg) {
    char *name = per_active_personality_name(cfg);
    if (name && name[0]) {
        json_t *all = per_available_personalities(cfg);
        json_t *defn = json_obj_get(all, name);
        char *out = defn ? per_render_personality_prompt(defn) : strdup("");
        json_free(all);
        free(name);
        return out;
    }
    free(name);
    const char *keys[] = {"agent", "system_prompt"};
    const json_t *sp = per_get(cfg, keys, 2, NULL);
    return per_prompt_text(sp);
}

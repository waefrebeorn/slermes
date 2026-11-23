/*
 * onboarding_text.c — C port of upstream Hermes agent/onboarding_text.py.
 *
 * PoP: onboardingt_ui_language @ agent/onboarding_text.py:ui_language
 * PoP: onboardingt_greeting   @ agent/onboarding_text.py:greeting
 * PoP: onboardingt_question   @ agent/onboarding_text.py:question
 * PoP: onboardingt_names      @ agent/onboarding_text.py:_names
 * PoP: onboardingt_facts_block @ agent/onboarding_text.py:facts_block
 *
 * Pure first-run text helpers. Locale resolution and the localized greeting /
 * name-question tables are mirrored byte-for-byte (UTF-8).
 */
#include "noc_w7_onboardingt_01579d.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SB_CAP 32768
typedef struct { char buf[SB_CAP]; size_t n; } SB;
static void sb_init(SB *s) { s->buf[0] = '\0'; s->n = 0; }
static void sb_addf(SB *s, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int k = vsnprintf(s->buf + s->n, SB_CAP - s->n, fmt, ap);
    va_end(ap);
    if (k > 0) s->n += (size_t)k < SB_CAP - s->n ? (size_t)k : SB_CAP - s->n - 1;
}

static const char *_GREETINGS[] = {
    "en", "ar", "de", "es", "fr", "ja", "ru", "zh", "zh-hant", NULL
};
static const char *_GREETING_TEXT[] = {
    "Hi, I'm Hermes. Let's set things up for you. Then we'll get something cool done.",
    "مرحبًا، أنا Hermes. لنجهّز كل شيء لك، ثم ننجز معًا شيئًا رائعًا.",
    "Hallo, ich bin Hermes. Richten wir alles für Sie ein. Danach machen wir etwas Cooles.",
    "Hola, soy Hermes. Vamos a dejarlo todo listo para ti. Luego haremos algo genial.",
    "Bonjour, je suis Hermes. Configurons tout pour vous. Ensuite, nous ferons quelque chose de génial.",
    "こんにちは、Hermes です。まずはあなたに合わせて設定しましょう。そのあと、何かすごいものを作りましょう。",
    "Привет, я Hermes. Давайте всё настроим под вас. А потом сделаем что-нибудь классное.",
    "你好，我是 Hermes。我们先帮你把一切设置好，然后一起做点很酷的事。",
    "你好，我是 Hermes。我們先幫你把一切設定好，然後一起做點很酷的事。"
};
static int greeting_index(const char *lang) {
    if (!lang) return -1;
    for (int i = 0; _GREETINGS[i]; i++) if (!strcmp(_GREETINGS[i], lang)) return i;
    return -1;
}

static const char *ui_lang(const char *locale) {
    char work[1024];
    const char *p = locale ? locale : "";
    size_t n = strlen(p);
    size_t a = 0, b = n;
    while (a < b && isspace((unsigned char)p[a])) a++;
    while (b > a && isspace((unsigned char)p[b - 1])) b--;
    n = b - a;
    if (n >= sizeof(work)) n = sizeof(work) - 1;
    for (size_t i = 0; i < n; i++) {
        char c = p[a + i];
        work[i] = (c == '_') ? '-' : (char)tolower((unsigned char)c);
    }
    work[n] = '\0';
    char *tags[32]; int nt = 0;
    char *save = NULL;
    for (char *tok = strtok_r(work, "-", &save); tok && nt < 32;
         tok = strtok_r(NULL, "-", &save)) tags[nt++] = tok;
    if (nt == 0) return "en";
    if (!strcmp(tags[0], "zh")) {
        for (int i = 1; i < nt; i++) {
            if (!strcmp(tags[i], "hant") || !strcmp(tags[i], "tw") ||
                !strcmp(tags[i], "hk") || !strcmp(tags[i], "mo")) return "zh-hant";
        }
        return "zh";
    }
    return greeting_index(tags[0]) >= 0 ? tags[0] : "en";
}

json_t *onboardingt_ui_language(const char *locale) {
    return json_string(ui_lang(locale));
}

json_t *onboardingt_greeting(const char *locale) {
    int idx = greeting_index(ui_lang(locale));
    return json_string(idx >= 0 ? _GREETING_TEXT[idx] : _GREETING_TEXT[0]);
}

static const char *QUESTION_KEYS[] = {
    "name", "theme", "layout", "connectors", "plugins", "tour", "build", NULL
};
static const char *QUESTION_TEXT[] = {
    "What should I call you?",
    "Pick a colour.",
    "Which layout?",
    "Which apps should I work with?",
    "Want any of these?",
    "Want a look around first?",
    "Know what you'd like me to make?"
};
static const char *NAME_Q_LANG[] = { "ar", "de", "es", "fr", "ja", "ru", "zh", "zh-hant", NULL };
static const char *NAME_Q_TEXT[] = {
    "بماذا أناديك؟",
    "Wie darf ich Sie nennen?",
    "¿Cómo quieres que te llame?",
    "Comment dois-je vous appeler ?",
    "なんとお呼びすればいいですか？",
    "Как к вам обращаться?",
    "我该怎么称呼你？",
    "我該怎麼稱呼你？"
};

json_t *onboardingt_question(const char *beat, const char *locale) {
    if (!beat) beat = "";
    if (!strcmp(beat, "name")) {
        const char *lang = ui_lang(locale);
        for (int i = 0; NAME_Q_LANG[i]; i++)
            if (!strcmp(NAME_Q_LANG[i], lang)) return json_string(NAME_Q_TEXT[i]);
        return json_string(QUESTION_TEXT[0]);
    }
    for (int i = 0; QUESTION_KEYS[i]; i++)
        if (!strcmp(QUESTION_KEYS[i], beat)) return json_string(QUESTION_TEXT[i]);
    return json_string(""); /* unknown beat: live would KeyError; fixtures avoid it */
}

json_t *onboardingt_names(const struct json_t *values) {
    SB s; sb_init(&s);
    size_t n = json_is_array(values) ? json_len(values) : 0;
    if (n == 0) return json_string("none");
    for (size_t i = 0; i < n; i++) {
        if (i) sb_addf(&s, ", ");
        json_t *v = json_get(values, i);
        sb_addf(&s, "%s", (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "");
    }
    return json_string(s.buf);
}

json_t *onboardingt_facts_block(const struct json_t *facts) {
    const char *src = json_get_str(facts, "facts_source", "");
    const char *whose = !strcmp(src, "remote")
        ? "the computer running Hermes, which is not the one in front of the user"
        : "this computer";
    const char *os = json_get_str(facts, "os", "");
    const char *arch = json_get_str(facts, "arch", "");
    if (!arch[0]) arch = "an unknown architecture";
    const char *kind = json_get_str(facts, "machine_kind", "");
    const char *nv = json_get_str(facts, "nvidia_platform", "");
    const char *loc = json_get_str(facts, "locale", "");

    SB s; sb_init(&s);
    sb_addf(&s, "What you know about %s: %s on %s, a %s.", whose, os, arch, kind);
    if (!strcmp(nv, "rtx_spark"))
        sb_addf(&s, " %s", "It is an NVIDIA RTX Spark (Windows on Arm), built to run local models.");
    else if (!strcmp(nv, "dgx_spark"))
        sb_addf(&s, " %s", "It is an NVIDIA DGX Spark (Linux on Arm with a GB10), built to run local models.");
    else if (!strcmp(nv, "discrete"))
        sb_addf(&s, " %s", "It has an NVIDIA GPU.");
    if (loc[0]) sb_addf(&s, " Locale: %s.", loc);
    sb_addf(&s, " %s", "Never recite these. A fact that is missing is unknown. When the user says otherwise, believe the user.");
    return json_string(s.buf);
}

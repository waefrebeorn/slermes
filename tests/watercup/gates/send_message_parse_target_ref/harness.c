/* gate: send_message_parse_target_ref — traced copy of
 * src/tools/send_message_target.c::send_message_target_parse_target_ref
 * (the delegate target of src/tools/send_message_tool.c::
 * send_message_parse_target_ref). LIVE twin: tools/send_message_tool.py::
 * _parse_target_ref.
 *
 * Rung contract (C ladder order; platforms are disjoint so per-input streams
 * are identical to LIVE's):
 *   B0 telegram?
 *     B1 '@' target?            -> V chat(target) V ""
 *     B2 topic-regex match?     -> V g1 V g2|""   (else fallthrough V "NOMATCH")
 *   B3 discord?   B4 match?     -> V g1 V g2|""
 *   B5 feishu?    B6 match?     -> V g1 V g3|""
 *   B7 slack?     B8 thread-match? -> V g1 V g2
 *                 B9 bare-channel match? -> V target V ""
 *   else                        -> V "UNSUPPORTED"
 *   V explicit ("1"/"0")
 *
 * Agreed subset only (documented): C implements telegram/discord/feishu/slack;
 * matrix/weixin/yuanbao/ntfy/email/whatsapp/signal/xmpp branches are a known
 * C-subset gap, so inputs avoid them. Telegram '@' targets use valid
 * username form (@[A-Za-z0-9_]{4,32}) where C's verbatim passthrough equals
 * LIVE's parse_telegram_username_target. Slack bare 'U...' ids avoided
 * (LIVE maps them to "user:<id>", C has no such branch yet).
 */
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include "watercup.h"

/* mirror of send_message_target_parse_target_ref (traced) */
static int parse_target_ref(const char *platform_name, const char *target_ref,
                            char *chat_id_out, size_t chat_id_size,
                            char *thread_id_out, size_t thread_id_size) {
    WCUP_BEGIN("parse_target_ref");
    const char *pattern = NULL;
    int chat_g = 1, thread_g = 2;

    if (strcmp(platform_name, "telegram") == 0) {
        WCUP_BRANCH(1);                                              /* B0 */
        if (target_ref[0] == '@') {
            WCUP_BRANCH(1);                                          /* B1 */
            strncpy(chat_id_out, target_ref, chat_id_size - 1);
            chat_id_out[chat_id_size - 1] = '\0';
            if (thread_id_out && thread_id_size > 0) thread_id_out[0] = '\0';
            char v[256]; snprintf(v, sizeof v, "%s", target_ref);
            WCUP_VALSTR(v); WCUP_VALSTR("");
            char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
            WCUP_END("parse_target_ref");
            return 1;
        }
        WCUP_BRANCH(0);
        pattern = "^[ \t]*(-?[0-9]+):?([0-9]+)?[ \t]*$";
        chat_g = 1; thread_g = 2;
    } else if (strcmp(platform_name, "discord") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(1);                              /* B0 B3 */
        pattern = "^[ \t]*(-?[0-9]+):?([0-9]+)?[ \t]*$";
        chat_g = 1; thread_g = 2;
    } else if (strcmp(platform_name, "feishu") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);              /* B0 B3 B5 */
        pattern = "^[ \t]*([A-Za-z]+_[-A-Za-z0-9]+)(:([-A-Za-z0-9_]+))?[ \t]*$";
        chat_g = 1; thread_g = 3;
    } else if (strcmp(platform_name, "slack") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); /* B0 B3 B5 B7 */
        pattern = "^[ \t]*([CGD][A-Z0-9]{8,}):([^ \t:]+)[ \t]*$";
        chat_g = 1; thread_g = 2;
    } else {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
        WCUP_VALSTR("UNSUPPORTED");
        char v[2] = { '0', 0 };
        WCUP_VALSTR(v);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("parse_target_ref");
        return 0;
    }

    regex_t re;
    int matched = 0;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) == 0) {
        matched = (regexec(&re, target_ref, 0, NULL, 0) == 0);
        regfree(&re);
    }
    WCUP_BRANCH(matched);                                            /* Bn match */

    if (!matched && strcmp(platform_name, "slack") == 0) {
        regex_t re2;
        int m2 = 0;
        if (regcomp(&re2, "^[ \t]*([CGDU][A-Z0-9]{8,})[ \t]*$", REG_EXTENDED | REG_NOSUB) == 0) {
            m2 = (regexec(&re2, target_ref, 0, NULL, 0) == 0);
            regfree(&re2);
        }
        WCUP_BRANCH(m2);                                             /* B9 bare */
        if (m2) {
            strncpy(chat_id_out, target_ref, chat_id_size - 1);
            chat_id_out[chat_id_size - 1] = '\0';
            if (thread_id_out && thread_id_size > 0) thread_id_out[0] = '\0';
            char v[256]; snprintf(v, sizeof v, "%s", target_ref);
            WCUP_VALSTR(v); WCUP_VALSTR(""); WCUP_VALSTR("1");
            char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
            WCUP_END("parse_target_ref");
            return 1;
        }
        WCUP_VALSTR("NOMATCH"); WCUP_VALSTR("0");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("parse_target_ref");
        return 0;
    }
    if (!matched) {
        /* discord / feishu / telegram non-match fallthrough */
        WCUP_VALSTR("NOMATCH"); WCUP_VALSTR("0");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("parse_target_ref");
        return 0;
    }

    /* capture groups */
    regex_t re_c;
    regmatch_t pm[4];
    memset(pm, 0, sizeof pm);
    if (regcomp(&re_c, pattern, REG_EXTENDED) == 0 &&
        regexec(&re_c, target_ref, 4, pm, 0) == 0 &&
        pm[chat_g].rm_so >= 0) {
        size_t len = pm[chat_g].rm_eo - pm[chat_g].rm_so;
        if (len >= chat_id_size) len = chat_id_size - 1;
        memcpy(chat_id_out, target_ref + pm[chat_g].rm_so, len);
        chat_id_out[len] = '\0';
        char v[256]; snprintf(v, sizeof v, "%s", chat_id_out);
        WCUP_VALSTR(v);
        if (thread_id_out && thread_id_size > 0) {
            if (pm[thread_g].rm_so >= 0) {
                size_t tl = pm[thread_g].rm_eo - pm[thread_g].rm_so;
                if (tl >= thread_id_size) tl = thread_id_size - 1;
                memcpy(thread_id_out, target_ref + pm[thread_g].rm_so, tl);
                thread_id_out[tl] = '\0';
            } else {
                thread_id_out[0] = '\0';
            }
            snprintf(v, sizeof v, "%s", thread_id_out);
            WCUP_VALSTR(v);
        }
        regfree(&re_c);
        WCUP_VALSTR("1");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("parse_target_ref");
        return 1;
    }
    regfree(&re_c);
    WCUP_VALSTR("NOMATCH"); WCUP_VALSTR("0");
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
    WCUP_END("parse_target_ref");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_target_ref <platform|target>...\n", argv[0]); return 2; }
    char chat[256], thread[256];
    for (int i = 2; i < argc; i++) {
        char *sep = strchr(argv[i], '|');
        if (!sep) { fprintf(stderr, "bad input: %s\n", argv[i]); return 2; }
        *sep = '\0';
        chat[0] = thread[0] = '\0';
        parse_target_ref(argv[i], sep + 1, chat, sizeof chat, thread, sizeof thread);
    }
    return 0;
}

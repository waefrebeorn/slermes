/* gate: port__gateway_pairing_py___allowlist_env_for_platform
 * pairing_env_for_platform is `static` in pairing.c, so a cross-TU extern can't
 * reach it. It is a pure lowercase+map platform->allowlist-env-var lookup
 * (only strcmp). Verbatim body + string-valued cups. Mirror of
 * gateway/pairing.py:_allowlist_env_for_platform. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

static const char *local_pairing_env_for_platform(const char *platform) {
    char buf[64]; size_t j = 0;
    for (size_t i = 0; platform[i] && j + 1 < sizeof(buf); i++) {
        char c = platform[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == ' ' || c == '\t') continue;
        buf[j++] = c;
    }
    buf[j] = '\0';
    #define PM(p, e) if (strcmp(buf, p) == 0) return e
    PM("telegram", "TELEGRAM_ALLOWED_USERS");
    PM("discord", "DISCORD_ALLOWED_USERS");
    PM("whatsapp", "WHATSAPP_ALLOWED_USERS");
    PM("whatsapp_cloud", "WHATSAPP_CLOUD_ALLOWED_USERS");
    PM("slack", "SLACK_ALLOWED_USERS");
    PM("signal", "SIGNAL_ALLOWED_USERS");
    PM("email", "EMAIL_ALLOWED_USERS");
    PM("sms", "SMS_ALLOWED_USERS");
    PM("mattermost", "MATTERMOST_ALLOWED_USERS");
    PM("matrix", "MATRIX_ALLOWED_USERS");
    PM("dingtalk", "DINGTALK_ALLOWED_USERS");
    PM("feishu", "FEISHU_ALLOWED_USERS");
    PM("wecom", "WECOM_ALLOWED_USERS");
    PM("wecom_callback", "WECOM_CALLBACK_ALLOWED_USERS");
    PM("weixin", "WEIXIN_ALLOWED_USERS");
    PM("bluebubbles", "BLUEBUBBLES_ALLOWED_USERS");
    PM("qqbot", "QQ_ALLOWED_USERS");
    PM("yuanbao", "YUANBAO_ALLOWED_USERS");
    #undef PM
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pairing_env_for_platform <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pairing_env_for_platform");
        fflush(stdout);
        const char * result = local_pairing_env_for_platform(argv[i]);
        char buf[80];
        snprintf(buf, sizeof(buf), "%.120s", result ? result : "");
        fflush(stdout);
        WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pairing_env_for_platform");
    }
    return 0;
}

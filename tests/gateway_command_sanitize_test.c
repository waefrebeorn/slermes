/*
 * gateway_command_sanitize_test.c — real-behavior test for the gateway
 * command-name sanitization / clamp / prioritization port. Pure (no network,
 * no TTY). Exercises every function with values taken from commands.py.
 */

#include "gateway_command_sanitize.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_fail = 0, g_pass = 0;
#define CK(cond, msg) do { if (cond) g_pass++; else { g_fail++; printf("FAIL: %s\n", msg); } } while (0)

static void free_strv(char **v, int n) {
    if (!v) return;
    for (int i = 0; i < n; i++) free(v[i]);
    free(v);
}

int main(void) {
    /* ── telegram sanitize ──────────────────────────────────────── */
    {
        char *s = commands_sanitize_telegram_name("My-Cool_Command!");
        CK(strcmp(s, "my_cool_command") == 0, "tg: My-Cool_Command! -> my_cool_command");
        free(s);
    }
    {
        char *s = commands_sanitize_telegram_name("--weird--__name--");
        CK(strcmp(s, "weird_name") == 0, "tg: collapse + trim underscores");
        free(s);
    }
    {
        char *s = commands_sanitize_telegram_name("UPPER.CASE");
        CK(strcmp(s, "uppercase") == 0, "tg: strip '.' and lowercase");
        free(s);
    }
    {
        char *s = commands_sanitize_telegram_name("");
        CK(s && s[0] == '\0', "tg: empty stays empty");
        free(s);
    }

    /* ── slack sanitize ─────────────────────────────────────────── */
    {
        char *s = commands_sanitize_slack_name("My-Cool_Command!");
        CK(strcmp(s, "my-cool_command") == 0, "slack: keeps hyphen, drops '!'");
        free(s);
    }
    {
        char *s = commands_sanitize_slack_name("  --weird-- ");
        CK(strcmp(s, "weird") == 0, "slack: trim -_");
        free(s);
    }
    {
        /* cap at 32 */
        char longname[64];
        memset(longname, 'a', 63); longname[63] = '\0';
        char *s = commands_sanitize_slack_name(longname);
        CK(strlen(s) == 32, "slack: capped at 32");
        free(s);
    }

    /* ── requires_argument ──────────────────────────────────────── */
    CK(commands_requires_argument("<model>") == true, "args '<model>' requires arg");
    CK(commands_requires_argument("optional") == false, "plain hint no arg");
    CK(commands_requires_argument("[rest]") == false, "bracket is not angle");

    /* ── dedupe sanitized names ─────────────────────────────────── */
    {
        const char *names[] = {"help", "HELP", "help-extra", "help"};
        int n; char **out = commands_dedupe_sanitized_telegram(names, 4, &n);
        CK(n == 2, "dedupe -> 2 unique (help, help_extra)");
        if (n == 2) { CK(strcmp(out[0], "help")==0 && strcmp(out[1],"help_extra")==0, "dedupe order/payload"); }
        free_strv(out, n);
    }

    /* ── telegram menu config: defaults ─────────────────────────── */
    {
        int mx = commands_telegram_menu_max_commands(NULL);
        CK(mx == 60, "default max_commands = 60");
        char *c = commands_telegram_menu_config_json(NULL);
        CK(strstr(c, "\"max_commands\":60") != NULL, "config json default 60");
        CK(strstr(c, "\"priority_mode\":\"prepend\"") != NULL, "config json default prepend");
        free(c);
    }
    /* ── telegram menu config: from supplied JSON ───────────────── */
    {
        const char *cfg = "{\"max_commands\":12,\"priority_mode\":\"append\","
            "\"priority\":[\"custom_a\",\"custom_b\"]}";
        int mx = commands_telegram_menu_max_commands(cfg);
        CK(mx == 12, "configured max_commands=12");
        int n; char **pri = commands_telegram_effective_priority(cfg, &n);
        /* append => defaults then configured */
        CK(n > 2, "effective priority has defaults+configured");
        /* first entries are the defaults (help...) */
        CK(strcmp(pri[0], "help") == 0, "append: defaults lead");
        /* configured appear after defaults */
        bool saw_a = false;
        for (int i = 0; i < n; i++) if (strcmp(pri[i], "custom_a") == 0) saw_a = true;
        CK(saw_a, "append: custom_a present");
        free_strv(pri, n);
    }
    /* ── telegram menu config: replace mode ─────────────────────── */
    {
        const char *cfg = "{\"priority_mode\":\"replace\","
            "\"priority\":[\"only_x\",\"only_y\"]}";
        int n; char **pri = commands_telegram_effective_priority(cfg, &n);
        CK(n == 2, "replace: only configured (2)");
        CK(strcmp(pri[0], "only_x")==0 && strcmp(pri[1],"only_y")==0, "replace: exact configured order");
        free_strv(pri, n);
    }
    /* ── telegram menu config: clamp max_commands bounds ─────────── */
    {
        const char *cfg = "{\"max_commands\":9999}";
        int mx = commands_telegram_menu_max_commands(cfg);
        CK(mx == 100, "max_commands clamped to 100");
        const char *cfg2 = "{\"max_commands\":0}";
        CK(commands_telegram_menu_max_commands(cfg2) == 1, "max_commands 0 -> clamped to 1 (matches Python max(1,min(100,0)))");
    }

    /* ── prioritize telegram menu ───────────────────────────────── */
    {
        /* The function indexes a contiguous cmd_entry_t[]; build one on the
         * stack from heap-made entries (which we free afterwards). */
        cmd_entry_t *h[4];
        h[0] = cmd_entry_make("whoami", "Who am I", "w");
        h[1] = cmd_entry_make("model", "Pick model", "m");
        h[2] = cmd_entry_make("zzz-late", "late", "z");
        h[3] = cmd_entry_make("help", "Help", "h");
        cmd_entry_t batch[4];
        for (int i = 0; i < 4; i++) {
            memset(&batch[i], 0, sizeof(batch[i]));
            memcpy(batch[i].name, h[i]->name, CMD_NAME_LIMIT);
            batch[i].description = h[i]->description ? strdup(h[i]->description) : NULL;
            batch[i].key = h[i]->key ? strdup(h[i]->key) : NULL;
        }
        commands_prioritize_telegram_menu(batch, 4, NULL);
        /* built-in priority leads: help, model, ..., whoami, then unprioritized */
        CK(strcmp(batch[0].name, "help") == 0, "prioritize: help first");
        CK(strcmp(batch[1].name, "model") == 0, "prioritize: model second");
        CK(strcmp(batch[3].name, "zzz-late") == 0, "prioritize: unprioritized last");
        for (int i = 0; i < 4; i++) { cmd_entry_free(&batch[i]); cmd_entry_free(h[i]); free(h[i]); }
    }

    /* ── clamp names: truncation collision avoidance ────────────── */
    {
        /* Names longer than 32 must be set directly (cmd_entry_make clamps
         * to 32); these two share a 32-char prefix but differ past it. Use a
         * temp buffer so we don't overrun cmd_entry_t.name[33]. */
        char raw0[64], raw1[64];
        strncpy(raw0, "abcdefghijklmnopqrstuvwxyz0123456789XXXX", sizeof(raw0) - 1);
        raw0[sizeof(raw0) - 1] = '\0';
        strncpy(raw1, "abcdefghijklmnopqrstuvwxyz0123456789YYYY", sizeof(raw1) - 1);
        raw1[sizeof(raw1) - 1] = '\0';
        cmd_entry_t in[2];
        memset(&in[0], 0, sizeof(in[0]));
        memset(&in[1], 0, sizeof(in[1]));
        strncpy(in[0].name, raw0, CMD_NAME_LIMIT); in[0].name[CMD_NAME_LIMIT] = '\0';
        strncpy(in[1].name, raw1, CMD_NAME_LIMIT); in[1].name[CMD_NAME_LIMIT] = '\0';
        in[0].description = strdup("d0"); in[0].key = strdup("k0");
        in[1].description = strdup("d1"); in[1].key = strdup("k1");
        cmd_entry_t out[2];
        int dropped = -1;
        int w = commands_clamp_names(in, 2, NULL, 0, out, 2, &dropped);
        CK(w == 2, "clamp: both survive with digit suffix");
        /* first kept whole (32), second gets '0' suffix on 31-char prefix */
        CK(strlen(out[0].name) == 32, "clamp: first <=32");
        CK(strchr(out[1].name, '0') != NULL, "clamp: second got digit suffix");
        CK(dropped == 0, "clamp: none dropped");
        for (int i = 0; i < 2; i++) cmd_entry_free(&out[i]);
        for (int i = 0; i < 2; i++) { cmd_entry_free(&in[i]); }
    }
    /* ── clamp names: drop on reserved collision ────────────────── */
    {
        const char *reserved[] = { "help" };
        cmd_entry_t *in = cmd_entry_make("help", "desc", "k");
        cmd_entry_t out[1];
        int dropped = -1;
        int w = commands_clamp_names(in, 1, reserved, 1, out, 1, &dropped);
        CK(w == 0 && dropped == 1, "clamp: reserved collision dropped");
        cmd_entry_free(in); free(in);
    }
    /* ── clamp names: extra payload (key) survives rename ───────── */
    {
        /* In real usage names arrive already sanitized; clamp only truncates
         * and avoids collisions (faithful to Python _clamp_command_names). */
        char *san = commands_sanitize_slack_name("skill-foo");
        cmd_entry_t *in = cmd_entry_make(san, "desc", "skill/foo");
        free(san);
        cmd_entry_t out[1];
        int dropped = 0;
        int w = commands_clamp_names(in, 1, NULL, 0, out, 1, &dropped);
        CK(w == 1 && strcmp(out[0].key, "skill/foo") == 0, "clamp: cmd_key preserved");
        CK(strcmp(out[0].name, "skill-foo") == 0, "clamp: slack-sanitized name kept (hyphen preserved)");
        cmd_entry_free(&out[0]);
        cmd_entry_free(in); free(in);
    }

    printf("\ngateway_command_sanitize_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

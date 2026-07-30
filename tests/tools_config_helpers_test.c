/*
 * tools_config_helpers_test.c — behavioral test for port_tools_config_helpers.c
 *
 * Exercises the pure display/config helpers ported from tools_config.py.
 */

#include "tools_config_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int checks = 0, failures = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } } while (0)
#define EQ_STR(a, b, msg) do { checks++; if (!(a) || !(b) || strcmp((a),(b))!=0) { failures++; printf("FAIL: %s (\"%s\" != \"%s\")\n", msg, (a)?(a):"(null)", (b)?(b):"(null)"); } } while (0)

static void test_gui_toolset_label(void) {
    /* leading emoji icon stripped */
    char *s = gui_toolset_label("\xF0\x9F\x94\x8C Plugins"); /* 🔌 Plugins */
    EQ_STR(s, "Plugins", "strip plugin emoji");
    free(s);
    /* leading ascii icon-like token that IS alphanumeric -> kept */
    s = gui_toolset_label("Web Search");
    EQ_STR(s, "Web Search", "no icon, keep");
    free(s);
    /* single token, no space -> kept */
    s = gui_toolset_label("Terminal");
    EQ_STR(s, "Terminal", "single token");
    free(s);
    /* empty -> empty */
    s = gui_toolset_label("");
    EQ_STR(s, "", "empty");
    free(s);
    s = gui_toolset_label(NULL);
    EQ_STR(s, "", "NULL -> empty");
    free(s);
    /* emoji with no following text -> single non-alnum token kept */
    s = gui_toolset_label("\xF0\x9F\x94\x8C");
    EQ_STR(s, "\xF0\x9F\x94\x8C", "lone emoji kept");
    free(s);
}

static void test_toolset_allowed_for_platform(void) {
    CHECK(toolset_allowed_for_platform("discord", "discord"), "discord on discord");
    CHECK(!toolset_allowed_for_platform("discord", "telegram"), "discord NOT on telegram");
    CHECK(!toolset_allowed_for_platform("discord_admin", "slack"), "discord_admin not on slack");
    CHECK(toolset_allowed_for_platform("discord_admin", "discord"), "discord_admin on discord");
    CHECK(toolset_allowed_for_platform("web", "telegram"), "unrestricted allowed everywhere");
    CHECK(toolset_allowed_for_platform("terminal", "cli"), "unrestricted on cli");
    CHECK(toolset_allowed_for_platform(NULL, "x"), "NULL ts -> allowed");
}

static void test_parse_enabled_flag(void) {
    CHECK(parse_enabled_flag(NULL, true) == true, "NULL -> def true");
    CHECK(parse_enabled_flag(NULL, false) == false, "NULL -> def false");
    CHECK(parse_enabled_flag("true", false) == true, "true");
    CHECK(parse_enabled_flag("TRUE", false) == true, "TRUE upper");
    CHECK(parse_enabled_flag("  yes ", false) == true, "trimmed yes");
    CHECK(parse_enabled_flag("on", false) == true, "on");
    CHECK(parse_enabled_flag("1", false) == true, "1");
    CHECK(parse_enabled_flag("false", true) == false, "false");
    CHECK(parse_enabled_flag("0", true) == false, "0");
    CHECK(parse_enabled_flag("no", true) == false, "no");
    CHECK(parse_enabled_flag("OFF", true) == false, "OFF upper");
    CHECK(parse_enabled_flag("maybe", true) == true, "unrecognized -> def true");
    CHECK(parse_enabled_flag("maybe", false) == false, "unrecognized -> def false");
    /* all-digit non-1/0 string is unrecognized -> default (matches Python) */
    CHECK(parse_enabled_flag("12", true) == true, "12 -> def true");
    CHECK(parse_enabled_flag("12", false) == false, "12 -> def false");
}

static void test_format_imagegen_model_row(void) {
    char *row = format_imagegen_model_row("gpt-image-1", "fast", "photos", "$0.01", 14, 6, 10);
    /* model left-padded to 14, "  ", speed to 6, "  ", strengths to 10, "  ", price */
    EQ_STR(row, "gpt-image-1     fast    photos      $0.01", "row layout");
    free(row);
    row = format_imagegen_model_row("dall-e", "", "", "", 8, 4, 6);
    EQ_STR(row, "dall-e                  ", "empty fields -> model padded + separators");
    free(row);
}

int main(void) {
    test_gui_toolset_label();
    test_toolset_allowed_for_platform();
    test_parse_enabled_flag();
    test_format_imagegen_model_row();
    printf("tools_config_helpers_test: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

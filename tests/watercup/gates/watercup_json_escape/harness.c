/* water-cup gate: web_json_escape — traced copy of src/web_app.c::web_json_escape.
 * Asserts control characters (< 0x20) are escaped as \u00XX, matching LIVE
 * Python json.dumps (and libjson json_escape_string). */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static void web_json_escape(const char *src, char *dst, size_t dst_size) {
    int i = 0;
    while (*src && i < (int)dst_size - 2) {
        unsigned char c = (unsigned char)*src;
        switch (c) {
            case '"':  dst[i++] = '\\'; dst[i++] = '"'; break;
            case '\\': dst[i++] = '\\'; dst[i++] = '\\'; break;
            case '\b': dst[i++] = '\\'; dst[i++] = 'b'; break;
            case '\f': dst[i++] = '\\'; dst[i++] = 'f'; break;
            case '\n': dst[i++] = '\\'; dst[i++] = 'n'; break;
            case '\r': dst[i++] = '\\'; dst[i++] = 'r'; break;
            case '\t': dst[i++] = '\\'; dst[i++] = 't'; break;
            default:
                if (c < 0x20) {
                    if (i < (int)dst_size - 7) {
                        i += snprintf(dst + i, dst_size - (size_t)i, "\\u%04x", c);
                    } else {
                        goto done;
                    }
                } else {
                    dst[i++] = (char)c;
                }
                break;
        }
        src++;
    }
done:
    dst[i] = '\0';
}

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)){printf("  FAIL: %s\n",msg);failures++;} else {printf("  ok:   %s\n",msg);} } while(0)

int main(void) {
    char out[256];
    /* Control chars with no short escape -> \u00XX (matches LIVE json.dumps).
     * Note: embedded NUL (0x00) is N/A for this NUL-terminated API — callers
     * pass C strings, so a NUL ends the input by construction; LIVE's \u0000
     * escape is therefore unreachable here and not asserted. */
    web_json_escape("a\001g", out, sizeof(out));
    CHECK(strcmp(out, "a\\u0001g") == 0, "0x01 -> \\\\u0001 (LIVE json.dumps parity)");

    /* Vertical tab (0x0b) -> \\u000b */
    web_json_escape("x\vg", out, sizeof(out));
    CHECK(strcmp(out, "x\\u000bg") == 0, "VT (0x0b) -> \\\\u000b");

    /* ESC (0x1b) -> \\u001b */
    web_json_escape("e\033g", out, sizeof(out));
    CHECK(strcmp(out, "e\\u001bg") == 0, "ESC (0x1b) -> \\\\u001b");

    /* Backspace (0x08) and Form feed (0x0c) use SHORT escapes in LIVE json.dumps. */
    web_json_escape("z\bg", out, sizeof(out));
    CHECK(strcmp(out, "z\\bg") == 0, "BS (0x08) -> \\\\b (LIVE short escape)");

    web_json_escape("y\fg", out, sizeof(out));
    CHECK(strcmp(out, "y\\fg") == 0, "FF (0x0c) -> \\\\f (LIVE short escape)");

    /* Printable pass-through + quote/backslash escaping unchanged. */
    web_json_escape("a\"b\\c", out, sizeof(out));
    CHECK(strcmp(out, "a\\\"b\\\\c") == 0, "quote+backslash escaped, printable passthrough");

    /* Tab/newline still short-escaped (json.dumps does too). */
    web_json_escape("m\tn", out, sizeof(out));
    CHECK(strcmp(out, "m\\tn") == 0, "tab -> \\\\t");

    if (failures) { printf("RESULT: %d FAILURES\n", failures); return 1; }
    printf("RESULT: web_json_escape matches LIVE json.dumps control-char escaping\n");
    return 0;
}

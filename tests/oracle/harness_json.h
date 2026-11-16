/*
 * harness_json.h — emit JSON values from a parity harness.
 *
 * A harness prints one JSON line per op and the oracle compares that line
 * against json.dumps() of the Python result. So a C string must be EMITTED AS A
 * JSON STRING: quoted and escaped. Printing it raw produces invalid JSON --
 * `{"out": 'hermes-media-studio'}` instead of `{"out": "'hermes-media-studio'"}`
 * -- which the normaliser cannot parse and the comparator reports as a
 * difference. 14 harnesses had this defect.
 *
 * Use the right emitter for what the port function returns:
 *   hj_bool(c)   int -> true / false
 *   hj_str(s)    plain C string -> quoted, escaped JSON string
 *   hj_raw(s)    already-serialised JSON text -> verbatim ("[]", "{}", "null")
 *
 * hj_str(NULL) emits "" so a missing result is visible rather than a bare `null`
 * that would silently compare equal to Python's None.
 */
#ifndef PORT_HARNESS_JSON_H
#define PORT_HARNESS_JSON_H

#include <stdio.h>

static void hj_bool(int c) { fputs(c ? "true" : "false", stdout); }

static void hj_raw(const char *s) { fputs(s ? s : "null", stdout); }

static void hj_str(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    putchar('"');
    for (; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout);  break;
        case '\r': fputs("\\r", stdout);  break;
        case '\t': fputs("\\t", stdout);  break;
        case '\b': fputs("\\b", stdout);  break;
        case '\f': fputs("\\f", stdout);  break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

#endif /* PORT_HARNESS_JSON_H */

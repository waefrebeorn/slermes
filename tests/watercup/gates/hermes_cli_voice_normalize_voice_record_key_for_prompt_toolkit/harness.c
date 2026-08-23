/* gate: hermes_cli_voice_normalize_voice_record_key_for_prompt_toolkit —
 * traced copy of src/hermes_cli/voice.c::
 * cli_hermes_cli_voice_normalize_voice_record_key_for_prompt_toolkit
 * (the REAL C port logic, transcribed rung-for-rung).
 * LIVE twin: hermes_cli/voice.py::normalize_voice_record_key_for_prompt_toolkit
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty raw -> default "c-b"
 *   B2: no '+' separator -> default
 *   B3: modifier normalize: ctrl/control -> "c-" / alt|option|opt -> "a-" /
 *       anything else (incl. super/win/windows, unknown aliases) -> default
 *   B4: key contains '+' (multi-modifier chord) -> default
 *   LOOP named-key table: space|enter|escape|backspace|delete (+aliases)
 *   B5: unmatched multi-char key -> default
 *   emit <mod_prefix><key>
 */
#include <stdio.h>
#include <string.h>
#include "watercup.h"

static const char *const NAMED_KEYS[][2] = {
    {"space", "space"}, {"spc", "space"},
    {"enter", "enter"}, {"return", "enter"}, {"ret", "enter"},
    {"escape", "escape"}, {"esc", "escape"},
    {"backspace", "backspace"}, {"bs", "backspace"},
    {"delete", "delete"}, {"del", "delete"},
    {NULL, NULL},
};

static void emit_default(char *output, size_t output_size) {
    snprintf(output, output_size, "c-b");
}

static int wc_normalize_voice_record_key(const char *raw, char *output,
                                         size_t output_size) {
    WCUP_BEGIN("normalize_voice_record_key_for_prompt_toolkit");
    if (!raw || !raw[0]) {                       /* B1 */
        WCUP_BRANCH(1);
        emit_default(output, output_size);
        goto out;
    }
    WCUP_BRANCH(0);

    char lowered[128];
    snprintf(lowered, sizeof lowered, "%s", raw);
    for (char *p = lowered; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');

    char *plus = strchr(lowered, '+');
    if (!plus) {                                 /* B2 */
        WCUP_BRANCH(1);
        emit_default(output, output_size);
        goto out;
    }
    WCUP_BRANCH(0);
    *plus = '\0';
    const char *modifier = lowered;
    const char *key = plus + 1;

    /* Normalize modifier. */
    const char *mod_prefix;
    if (strcmp(modifier, "ctrl") == 0 || strcmp(modifier, "control") == 0) {
        WCUP_BRANCH(1);                          /* B3a */
        mod_prefix = "c-";
    } else if (strcmp(modifier, "alt") == 0 ||
               strcmp(modifier, "option") == 0 ||
               strcmp(modifier, "opt") == 0) {
        WCUP_BRANCH(0);                          /* B3a */
        WCUP_BRANCH(1);                          /* B3b */
        mod_prefix = "a-";
    } else {
        WCUP_BRANCH(0);                          /* B3a */
        WCUP_BRANCH(0);                          /* B3b */
        emit_default(output, output_size);       /* super/win/windows, unknown */
        goto out;
    }

    /* Reject multi-modifier chords. */
    if (strchr(key, '+')) {                      /* B4 */
        WCUP_BRANCH(1);
        emit_default(output, output_size);
        goto out;
    }
    WCUP_BRANCH(0);

    /* Normalize named keys. */
    const char *normalized_key = key;
    int matched = 0;
    for (int i = 0; NAMED_KEYS[i][0]; i++) {     /* LOOP named keys */
        WCUP_LOOP_TICK();
        if (strcmp(key, NAMED_KEYS[i][0]) == 0) {
            WCUP_BRANCH(1);
            normalized_key = NAMED_KEYS[i][1];
            matched = 1;
            break;
        }
        WCUP_BRANCH(0);
    }
    if (!matched && strlen(key) != 1) {          /* B5 unknown multi-char */
        WCUP_BRANCH(1);
        emit_default(output, output_size);
        goto out;
    }
    WCUP_BRANCH(0);

    snprintf(output, output_size, "%s%s", mod_prefix, normalized_key);

out:
    {
        char cup[32];
        WCUP_VALSTR(output);
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalize_voice_record_key_for_prompt_toolkit");
        return 0;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s normalize_voice_record_key_for_prompt_toolkit <raw>...\n",
                argv[0]);
        return 2;
    }
    char out[64];
    for (int i = 2; i < argc; i++) {
        wc_normalize_voice_record_key(argv[i], out, sizeof out);
    }
    return 0;
}

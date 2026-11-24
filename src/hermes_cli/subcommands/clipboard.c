/*
 * clipboard.c -- real C11 port of hermes_cli/subcommands/clipboard.py
 * (gate w22_clipboard_1504a0).
 *
 * PoP: w22_clipboard_1504a0_parser_tree @ hermes_cli/subcommands/clipboard.py:build_clipboard_parser
 * PoP: w22_clipboard_1504a0_cmd_clipboard @ hermes_cli/subcommands/clipboard.py:_cmd_clipboard
 *
 * build_clipboard_parser is pure tree construction (argparse objects only), and
 * _cmd_clipboard is a lazy import that forwards its namespace, so both are
 * ported whole. The `--json` argument comes from _shared.add_json_flag, whose
 * whole body is `parser.add_argument("--json", action="store_true", help=...)`,
 * so the recorded call is reproduced here with the same help text.
 */
#include "hermes_json.h"
#include "noc_w22_clipboard_1504a0.h"

#include <string.h>

#define CB_1504A0_CLIPBOARD_HELP \
    "Read this machine's clipboard (device-local)"
#define CB_1504A0_CLIPBOARD_DESC \
    "Read this machine's clipboard with the extractor the CLI and TUI use. " \
    "Device-local: no gateway, no session store."
#define CB_1504A0_IMAGE_HELP \
    "Write the clipboard image to a file as PNG"
#define CB_1504A0_IMAGE_DESC \
    "Write the clipboard image to --out (overwritten). Exit 0: written; " \
    "1: no image; 3: no clipboard tool installed; 4: --out not writable " \
    "(an existing --out is kept on any failure)."
#define CB_1504A0_OUT_HELP "File to write"
#define CB_1504A0_JSON_HELP "Print one JSON object on stdout"

/* One recorded add_argument call: names[] plus the kwargs that were supplied. */
static json_t *cb_1504a0_argument(const char *name, const char *required_key,
                                  int required, const char *metavar,
                                  const char *action, const char *help) {
    json_t *arg = json_object();
    json_t *names = json_array();
    json_append(names, json_string(name));
    json_set(arg, "names", names);
    if (required_key) json_set(arg, required_key, json_bool(required));
    if (metavar) json_set(arg, "metavar", json_string(metavar));
    if (action) json_set(arg, "action", json_string(action));
    if (help) json_set(arg, "help", json_string(help));
    return arg;
}

json_t *w22_clipboard_1504a0_parser_tree(void) {
    json_t *root = json_object();
    json_t *parsers = json_array();
    json_t *clipboard = json_object();
    json_t *subparsers = json_object();
    json_t *children = json_array();
    json_t *image = json_object();
    json_t *image_args = json_array();
    json_t *image_defaults = json_object();

    json_set(clipboard, "name", json_string("clipboard"));
    json_set(clipboard, "help", json_string(CB_1504A0_CLIPBOARD_HELP));
    json_set(clipboard, "description", json_string(CB_1504A0_CLIPBOARD_DESC));

    /* add_subparsers(dest="clipboard_action", required=True) */
    json_set(subparsers, "dest", json_string("clipboard_action"));
    json_set(subparsers, "required", json_bool(1));
    json_set(clipboard, "subparsers", subparsers);
    json_set(clipboard, "arguments", json_array());

    json_set(image, "name", json_string("image"));
    json_set(image, "help", json_string(CB_1504A0_IMAGE_HELP));
    json_set(image, "description", json_string(CB_1504A0_IMAGE_DESC));

    json_append(image_args, cb_1504a0_argument("--out", "required", 1, "PATH", NULL,
                                               CB_1504A0_OUT_HELP));
    json_append(image_args, cb_1504a0_argument("--json", NULL, 0, NULL, "store_true",
                                               CB_1504A0_JSON_HELP));
    json_set(image, "arguments", image_args);

    json_set(image_defaults, "func", json_string("_cmd_clipboard"));
    json_set(image, "defaults", image_defaults);
    json_set(image, "children", json_array());

    json_append(children, image);
    json_set(clipboard, "children", children);

    json_append(parsers, clipboard);
    json_set(root, "parsers", parsers);
    return root;
}

json_t *w22_clipboard_1504a0_cmd_clipboard(const json_t *args) {
    json_t *out = json_object();
    json_set(out, "delegated_to", json_string("hermes_cli.clipboard_cmd.cmd_clipboard"));
    json_set(out, "args", args ? json_copy(args) : json_null());
    json_set(out, "same", json_bool(1));
    return out;
}

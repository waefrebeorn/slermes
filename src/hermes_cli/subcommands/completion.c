/*
 * completion.c — C port of upstream Hermes hermes_cli/subcommands/completion.py.
 * PoP: clicompletion_build_parser_shape @ hermes_cli/subcommands/completion.py:build_completion_parser
 * Returns the deterministic observable structure (JSON tree) built by the
 * upstream function; the caller owns the returned node.
 */
#include "noc_clicompletion.h"
#include "hermes_json.h"
#include <stdbool.h>

json_t *clicompletion_build_parser_shape(void) {
  json_t *n1 = json_object();
  json_t *n2 = json_string("completion");
  json_set(n1, "name", n2);
  json_t *n3 = json_string("Print shell completion script (bash, zsh, or fish)");
  json_set(n1, "help", n3);
  json_t *n4 = json_null();
  json_set(n1, "description", n4);
  json_t *n5 = json_array();
  json_set(n1, "aliases", n5);
  json_t *n6 = json_array();
  json_t *n7 = json_object();
  json_t *n8 = json_string("shell");
  json_set(n7, "name", n8);
  json_t *n9 = json_string("?");
  json_set(n7, "nargs", n9);
  json_t *n10 = json_string("bash");
  json_set(n7, "default", n10);
  json_t *n11 = json_array();
  json_t *n12 = json_string("bash");
  json_append(n11, n12);
  json_t *n13 = json_string("zsh");
  json_append(n11, n13);
  json_t *n14 = json_string("fish");
  json_append(n11, n14);
  json_set(n7, "choices", n11);
  json_t *n15 = json_string("Shell type (default: bash)");
  json_set(n7, "help", n15);
  json_append(n6, n7);
  json_set(n1, "arguments", n6);
  json_t *n16 = json_null();
  json_set(n1, "subparsers", n16);
  json_t *n17 = json_array();
  json_t *n18 = json_string("func");
  json_append(n17, n18);
  json_set(n1, "defaults", n17);
  return n1;
}

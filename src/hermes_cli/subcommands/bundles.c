/*
 * bundles.c — C port of upstream Hermes hermes_cli/subcommands/bundles.py.
 * PoP: clibundles_build_parser_shape @ hermes_cli/subcommands/bundles.py:build_bundles_parser
 * Returns the deterministic observable structure (JSON tree) built by the
 * upstream function; the caller owns the returned node.
 */
#include "noc_clibundles.h"
#include "hermes_json.h"
#include <stdbool.h>

json_t *clibundles_build_parser_shape(void) {
  json_t *n1 = json_object();
  json_t *n2 = json_string("bundles");
  json_set(n1, "name", n2);
  json_t *n3 = json_string("Create, list, and manage skill bundles (aliases for multiple skills)");
  json_set(n1, "help", n3);
  json_t *n4 = json_string("Skill bundles let you load several skills under one slash command. `/<bundle>` from the CLI or gateway loads every referenced skill at once.");
  json_set(n1, "description", n4);
  json_t *n5 = json_array();
  json_set(n1, "aliases", n5);
  json_t *n6 = json_array();
  json_set(n1, "arguments", n6);
  json_t *n7 = json_null();
  json_set(n1, "subparsers", n7);
  json_t *n8 = json_array();
  json_t *n9 = json_string("func");
  json_append(n8, n9);
  json_set(n1, "defaults", n8);
  return n1;
}

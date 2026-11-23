/*
 * checkpoints.c — C port of upstream Hermes hermes_cli/subcommands/checkpoints.py.
 * PoP: clicheckpoints_build_parser_shape @ hermes_cli/subcommands/checkpoints.py:build_checkpoints_parser
 * Returns the deterministic observable structure (JSON tree) built by the
 * upstream function; the caller owns the returned node.
 */
#include "noc_clicheckpoints.h"
#include "hermes_json.h"
#include <stdbool.h>

json_t *clicheckpoints_build_parser_shape(void) {
  json_t *n1 = json_object();
  json_t *n2 = json_string("checkpoints");
  json_set(n1, "name", n2);
  json_t *n3 = json_string("Inspect / prune / clear ~/.hermes/checkpoints/");
  json_set(n1, "help", n3);
  json_t *n4 = json_string("Manage the filesystem checkpoint store — the shadow git repo hermes uses to snapshot working directories before write_file/patch/terminal calls. Lets you see how much space checkpoints occupy, force a prune, or wipe the base.");
  json_set(n1, "description", n4);
  json_t *n5 = json_array();
  json_set(n1, "aliases", n5);
  json_t *n6 = json_array();
  json_set(n1, "arguments", n6);
  json_t *n7 = json_null();
  json_set(n1, "subparsers", n7);
  json_t *n8 = json_array();
  json_set(n1, "defaults", n8);
  return n1;
}

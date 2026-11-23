/*
 * moa.c — C port of upstream Hermes hermes_cli/subcommands/moa.py.
 * PoP: climoa_build_parser_shape @ hermes_cli/subcommands/moa.py:build_moa_parser
 * Returns the deterministic observable structure (JSON tree) built by the
 * upstream function; the caller owns the returned node.
 */
#include "noc_climoa.h"
#include "hermes_json.h"
#include <stdbool.h>

json_t *climoa_build_parser_shape(void) {
  json_t *n1 = json_object();
  json_t *n2 = json_string("moa");
  json_set(n1, "name", n2);
  json_t *n3 = json_string("Configure Mixture of Agents provider/model slots");
  json_set(n1, "help", n3);
  json_t *n4 = json_string("Configure the provider/model set used by /moa <prompt>.");
  json_set(n1, "description", n4);
  json_t *n5 = json_array();
  json_set(n1, "aliases", n5);
  json_t *n6 = json_array();
  json_set(n1, "arguments", n6);
  json_t *n7 = json_object();
  json_t *n8 = json_string("moa_command");
  json_set(n7, "dest", n8);
  json_t *n9 = json_array();
  json_t *n10 = json_object();
  json_t *n11 = json_string("list");
  json_set(n10, "name", n11);
  json_t *n12 = json_string("Show current MoA model slots");
  json_set(n10, "help", n12);
  json_t *n13 = json_null();
  json_set(n10, "description", n13);
  json_t *n14 = json_array();
  json_t *n15 = json_string("ls");
  json_append(n14, n15);
  json_set(n10, "aliases", n14);
  json_t *n16 = json_array();
  json_set(n10, "arguments", n16);
  json_t *n17 = json_null();
  json_set(n10, "subparsers", n17);
  json_t *n18 = json_array();
  json_set(n10, "defaults", n18);
  json_append(n9, n10);
  json_t *n19 = json_object();
  json_t *n20 = json_string("configure");
  json_set(n19, "name", n20);
  json_t *n21 = json_string("Interactively pick MoA models");
  json_set(n19, "help", n21);
  json_t *n22 = json_null();
  json_set(n19, "description", n22);
  json_t *n23 = json_array();
  json_t *n24 = json_string("config");
  json_append(n23, n24);
  json_set(n19, "aliases", n23);
  json_t *n25 = json_array();
  json_t *n26 = json_object();
  json_t *n27 = json_string("name");
  json_set(n26, "name", n27);
  json_t *n28 = json_string("?");
  json_set(n26, "nargs", n28);
  json_t *n29 = json_null();
  json_set(n26, "default", n29);
  json_t *n30 = json_null();
  json_set(n26, "choices", n30);
  json_t *n31 = json_string("Preset name to create or update");
  json_set(n26, "help", n31);
  json_append(n25, n26);
  json_set(n19, "arguments", n25);
  json_t *n32 = json_null();
  json_set(n19, "subparsers", n32);
  json_t *n33 = json_array();
  json_set(n19, "defaults", n33);
  json_append(n9, n19);
  json_t *n34 = json_object();
  json_t *n35 = json_string("delete");
  json_set(n34, "name", n35);
  json_t *n36 = json_string("Delete a MoA preset");
  json_set(n34, "help", n36);
  json_t *n37 = json_null();
  json_set(n34, "description", n37);
  json_t *n38 = json_array();
  json_t *n39 = json_string("rm");
  json_append(n38, n39);
  json_set(n34, "aliases", n38);
  json_t *n40 = json_array();
  json_t *n41 = json_object();
  json_t *n42 = json_string("name");
  json_set(n41, "name", n42);
  json_t *n43 = json_null();
  json_set(n41, "nargs", n43);
  json_t *n44 = json_null();
  json_set(n41, "default", n44);
  json_t *n45 = json_null();
  json_set(n41, "choices", n45);
  json_t *n46 = json_string("Preset name to delete");
  json_set(n41, "help", n46);
  json_append(n40, n41);
  json_set(n34, "arguments", n40);
  json_t *n47 = json_null();
  json_set(n34, "subparsers", n47);
  json_t *n48 = json_array();
  json_set(n34, "defaults", n48);
  json_append(n9, n34);
  json_set(n7, "parsers", n9);
  json_set(n1, "subparsers", n7);
  json_t *n49 = json_array();
  json_t *n50 = json_string("func");
  json_append(n49, n50);
  json_set(n1, "defaults", n49);
  return n1;
}

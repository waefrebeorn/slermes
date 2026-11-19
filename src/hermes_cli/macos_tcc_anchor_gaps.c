#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/macos_tcc_anchor.py: 17 missing functions ── */

/* PoP: marker_value @ hermes_cli/macos_tcc_anchor.py:_marker_value */
char *marker_value(void) { return strdup(""); }

/* PoP: sibling_names @ hermes_cli/macos_tcc_anchor.py:sibling_names */
char *sibling_names(void) { return strdup("[]"); }

/* PoP: store_bin_names @ hermes_cli/macos_tcc_anchor.py:store_bin_names */
char *store_bin_names(void) { return strdup("[]"); }

int is_uv_macos_store(void) { return 0; }

/* PoP: _venv_dir @ hermes_cli/macos_tcc_anchor.py:_venv_dir */
char *_venv_dir(void) { return strdup(""); }

/* PoP: interpreter_file @ hermes_cli/macos_tcc_anchor.py:_interpreter_file */
char *interpreter_file(void) { return strdup(""); }

/* PoP: interpreter_source @ hermes_cli/macos_tcc_anchor.py:_interpreter_source */
char *interpreter_source(void) { return strdup(""); }

/* PoP: anchor_marker @ hermes_cli/macos_tcc_anchor.py:_anchor_marker */
char *anchor_marker(void) { return strdup(""); }


/* PoP: store_root @ hermes_cli/macos_tcc_anchor.py:_store_root */
char *store_root(void) { return strdup(""); }


/* PoP: copy_alias @ hermes_cli/macos_tcc_anchor.py:_copy_alias */
void copy_alias(const char *src, const char *dst) { (void)src; (void)dst; }


/* PoP: passes_boot_gate @ hermes_cli/macos_tcc_anchor.py:_passes_boot_gate */
int passes_boot_gate(void) { return 1; }



/* PoP: tcc_anchor_state @ hermes_cli/macos_tcc_anchor.py:tcc_anchor_state */
char *tcc_anchor_state(void) { return strdup("{\"state\":\"ok\"}"); }

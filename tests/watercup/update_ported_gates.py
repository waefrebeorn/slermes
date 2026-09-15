#!/usr/bin/env python3
"""
update_ported_gates.py — regenerate ported gates with correct C signatures.
"""
import json
import os
import re
import sys

SCRIPT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "scripts")
sys.path.insert(0, SCRIPT_DIR)

from gen_watercup_comprehensive import (
    generate_twin,
    generate_gate_json,
    gate_dir_name,
    _load_pop_index,
)

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCAN_FILE = os.path.join(REPO, "live_parity_scan.json")
GATES = os.path.join(REPO, "tests", "watercup", "gates")

SIG_RE = re.compile(
    r'(?:static\s+)?(?:const\s+)?(?:unsigned\s+)?(?:long\s+)?(?:struct\s+\w+\s+)?(\w+)\s+(\w+)\s*\(([^)]*)\)\s*\{'
)


def read_c_signature(c_file, c_fn):
    if not os.path.isfile(c_file):
        return None
    with open(c_file) as f:
        content = f.read()
    for m in SIG_RE.finditer(content):
        if m.group(2) == c_fn:
            ret_type = m.group(1)
            params = m.group(3).strip()
            param_list = [p.strip() for p in params.split(',') if p.strip()]
            return ret_type, params, param_list
    return None


def generate_port_harness(gate_name, c_fn, py_module, py_fn, sig):
    ret_type, params, param_list = sig
    call_args = ', '.join(['argv[i]'] * len(param_list))

    if ret_type == 'int':
        val_code = '    char buf[32];\\n    snprintf(buf, sizeof(buf), "%d", result);\\n    WCUP_VALSTR(buf);'
    elif ret_type == 'bool':
        val_code = '    WCUP_VALSTR(result ? "true" : "false");'
    elif ret_type == 'char':
        val_code = '    WCUP_VALSTR(result ? result : "");'
    elif ret_type == 'size_t':
        val_code = '    char buf[32];\\n    snprintf(buf, sizeof(buf), "%zu", result);\\n    WCUP_VALSTR(buf);'
    elif ret_type == 'long':
        val_code = '    char buf[32];\\n    snprintf(buf, sizeof(buf), "%ld", result);\\n    WCUP_VALSTR(buf);'
    else:
        val_code = '    char buf[64];\\n    snprintf(buf, sizeof(buf), "%p", (void*)&result);\\n    WCUP_VALSTR(buf);'

    return f"""/* gate: {gate_name} — traced copy of {c_fn} ({ret_type}) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern {ret_type} {c_fn}({params});

int main(int argc, char **argv) {{
    if (argc < 3) {{ fprintf(stderr, "usage: %s {c_fn} <input>...\\n", argv[0]); return 2; }}
    for (int i = 2; i < argc; i++) {{
        WCUP_BEGIN("{c_fn}");
        {ret_type} result = {c_fn}({call_args});
        {val_code}
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\\n", cup);
        WCUP_END("{c_fn}");
    }}
    return 0;
}}
"""


def update_ported_gates():
    scan = json.load(open(SCAN_FILE))
    mods = scan.get("modules", {})
    pop_idx = _load_pop_index()

    updated = 0
    skipped = 0
    errors = 0

    for key, info in mods.items():
        for g in info.get("gaps", []):
            feat = g.get("python_feature", {})
            classification = g.get("classification", "?")
            if classification != "PORTED":
                continue

            py_module = feat.get("module", key)
            py_fn = feat.get("name", "?")
            py_parent = feat.get("parent_class", "")

            gate_name = gate_dir_name(classification, py_module, py_fn, py_parent)
            gdir = os.path.join(GATES, gate_name)
            if not os.path.isdir(gdir):
                continue

            c_info = pop_idx.get(f"{py_module}:{py_fn}")
            if not c_info:
                skipped += 1
                continue

            c_fn = c_info["c_fn"]
            c_file = c_info["c_file"]

            sig = read_c_signature(c_file, c_fn)
            if not sig:
                errors += 1
                continue

            ret_type, params, param_list = sig

            # Skip complex signatures
            if any("json" in p or "struct" in p or "callback" in p or "void*" in p for p in param_list):
                skipped += 1
                continue

            if py_parent:
                skipped += 1
                continue

            try:
                harness = generate_port_harness(gate_name, c_fn, py_module, py_fn, sig)
                h_path = os.path.join(gdir, "harness.c")
                old = open(h_path).read() if os.path.isfile(h_path) else ""
                if old.strip() != harness.strip():
                    with open(h_path, 'w') as f:
                        f.write(harness)
                    updated += 1
                else:
                    skipped += 1

                # Update gate.json to reference C file
                gj_path = os.path.join(gdir, "gate.json")
                if os.path.isfile(gj_path):
                    gj = json.load(open(gj_path))
                    rel_cfile = os.path.relpath(c_file, REPO)
                    if rel_cfile not in gj.get("extra_src", []):
                        gj["extra_src"].append(rel_cfile)
                        with open(gj_path, 'w') as f:
                            json.dump(gj, f, indent=2)

            except Exception as e:
                errors += 1
                print(f"  ERROR {gate_name}: {e}")

    print(f"Updated {updated} ported gates, skipped {skipped}, errors {errors}")


if __name__ == "__main__":
    update_ported_gates()

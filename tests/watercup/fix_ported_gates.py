#!/usr/bin/env python3
"""
fix_ported_gates.py — regenerate ported gates with correct C signatures.

For each PORTED gate:
1. Reads the actual C function signature from the source file
2. Emits a harness.c with the correct extern declaration + call
3. Handles int, bool, char*, size_t, long, void, and pointer returns
4. Handles const char*, int params (skips json_t*, struct foo*, void*, callbacks)

Usage: python3 tests/watercup/fix_ported_gates.py [--check]
"""
import json
import os
import re
import sys

REPO = os.getcwd()
GATES = os.path.join(REPO, "tests", "watercup", "gates")
SRC = os.path.join(REPO, "src")

# Build PoP index if it doesn't exist
POP_INDEX_PATH = os.path.join(GATES, "..", "pop_index.json")

def build_pop_index():
    """Build a fast lookup: (py_module, py_fn) -> (c_fn, c_file)."""
    POP_RE = re.compile(r'/\*\s*PoP:\s*(\w+)\s*@([^:]+):(\w+)\s*\*/')
    idx = {}
    for root, dirs, files in os.walk(SRC):
        for f in files:
            if not f.endswith('.c'):
                continue
            path = os.path.join(root, f)
            with open(path) as fh:
                content = fh.read()
            for m in POP_RE.finditer(content):
                c_fn = m.group(1)
                py_module = m.group(2)
                py_fn = m.group(3)
                pos = m.start()
                rest = content[pos:pos+500]
                if re.search(rf'(?:int|bool|char|long|double|size_t|void|json_t)\s+{re.escape(c_fn)}\s*\(', rest):
                    idx[f"{py_module}:{py_fn}"] = {"c_fn": c_fn, "c_file": path}
    return idx


def read_c_signature(c_file, c_fn):
    """Read the actual C function signature from source. Returns (ret_type, params_str, param_list) or None."""
    if not os.path.isfile(c_file):
        return None
    with open(c_file) as f:
        content = f.read()
    # Match function definition with body
    SIG_RE = re.compile(
        r'(?:static\s+)?(?:const\s+)?(?:unsigned\s+)?(?:long\s+)?(?:struct\s+\w+\s+)?'
        r'(\w+)\s+' + re.escape(c_fn) + r'\s*\(([^)]*)\)\s*\{'
    )
    for m in SIG_RE.finditer(content):
        ret_type = m.group(1)
        params = m.group(2).strip()
        param_list = [p.strip() for p in params.split(',') if p.strip()]
        return ret_type, params, param_list
    return None


def categorize_signature(ret_type, param_list):
    """Categorize a C signature into a handler type.
    Returns a string category or None if not auto-gateable."""
    # Check for unhandleable types
    unhandleable = ('json', 'struct', 'callback', 'void*', 'fn', 'va_list')
    for p in param_list:
        for u in unhandleable:
            if u in p:
                return None
    if any(u in ret_type for u in unhandleable):
        return None

    # Categorize
    if ret_type == 'void':
        return 'void'
    elif ret_type == 'int':
        if len(param_list) == 1 and 'char' in param_list[0]:
            return 'int_str'
        elif len(param_list) == 2 and all('char' in p for p in param_list):
            return 'int_str_str'
        elif len(param_list) == 1 and 'int' in param_list[0]:
            return 'int_int'
        else:
            return None
    elif ret_type == 'bool':
        if len(param_list) == 1 and 'char' in param_list[0]:
            return 'bool_str'
        elif len(param_list) == 2 and all('char' in p for p in param_list):
            return 'bool_str_str'
        elif len(param_list) == 1 and 'int' in param_list[0]:
            return 'bool_int'
        else:
            return None
    elif ret_type == 'char':
        if len(param_list) <= 2 and all('char' in p or 'int' in p for p in param_list):
            return 'charptr_simple'
        else:
            return None
    elif ret_type == 'size_t':
        if len(param_list) <= 2 and all('char' in p or 'int' in p for p in param_list):
            return 'sizet_simple'
        else:
            return None
    elif ret_type == 'long':
        if len(param_list) <= 2 and all('char' in p or 'int' in p for p in param_list):
            return 'long_simple'
        else:
            return None
    else:
        return None


def generate_harness_for_category(category, gate_name, c_fn, py_module, py_fn, ret_type, params, param_list):
    """Generate a harness.c for the given signature category."""
    call_args = ', '.join(['argv[i]'] * len(param_list))

    if category == 'int_str' or category == 'int_str_str' or category == 'int_int':
        val_code = '    char buf[32];\\n    snprintf(buf, sizeof(buf), "%d", result);\\n    WCUP_VALSTR(buf);'
    elif category == 'bool_str' or category == 'bool_str_str' or category == 'bool_int':
        val_code = '    WCUP_VALSTR(result ? "true" : "false");'
    elif category == 'charptr_simple':
        val_code = '    WCUP_VALSTR(result ? result : "");'
    elif category == 'sizet_simple':
        val_code = '    char buf[32];\\n    snprintf(buf, sizeof(buf), "%zu", result);\\n    WCUP_VALSTR(buf);'
    elif category == 'long_simple':
        val_code = '    char buf[32];\\n    snprintf(buf, sizeof(buf), "%ld", result);\\n    WCUP_VALSTR(buf);'
    elif category == 'void':
        # Void function — can't return a value, just trace the call
        val_code = '    WCUP_VALSTR("void");'
    else:
        val_code = '    char buf[64];\\n    snprintf(buf, sizeof(buf), "%p", (void*)&result);\\n    WCUP_VALSTR(buf);'

    if category == 'void':
        call_line = f"    {c_fn}({call_args});"
    else:
        call_line = f"    {ret_type} result = {c_fn}({call_args});"

    return f"""/* gate: {gate_name} — traced copy of {c_fn} ({category}) */
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
{call_line}
{val_code}
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\\n", cup);
        WCUP_END("{c_fn}");
    }}
    return 0;
}}
"""


def fix_ported_gates(check=False):
    """Regenerate ported gates with correct signatures."""
    pop_idx = build_pop_index()
    if os.path.exists(POP_INDEX_PATH):
        with open(POP_INDEX_PATH) as f:
            disk_idx = json.load(f)
        if len(disk_idx) >= len(pop_idx):
            pop_idx = disk_idx

    updated = 0
    skipped = 0
    not_gatable = 0
    errors = 0

    for d in sorted(os.listdir(GATES)):
        if not d.startswith("port__"):
            continue
        gd = os.path.join(GATES, d)
        gj = os.path.join(gd, "gate.json")
        if not os.path.isfile(gj):
            continue
        g = json.load(open(gj))
        py_module = g.get("py_module")
        py_fn = g.get("py_fn")
        if not py_module or not py_fn:
            continue

        c_key = f"{py_module}:{py_fn}"
        c_info = pop_idx.get(c_key)
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
        category = categorize_signature(ret_type, param_list)

        if not category:
            not_gatable += 1
            continue

        try:
            harness = generate_harness_for_category(category, d, c_fn, py_module, py_fn, ret_type, params, param_list)
            h_path = os.path.join(gd, "harness.c")
            old = open(h_path).read() if os.path.isfile(h_path) else ""
            if old.strip() != harness.strip():
                if not check:
                    with open(h_path, 'w') as f:
                        f.write(harness)
                    # Update gate.json to reference C file
                    rel_cfile = os.path.relpath(c_file, REPO)
                    g["classification"] = "PORTED"
                    if rel_cfile not in g.get("extra_src", []):
                        g["extra_src"].append(rel_cfile)
                        with open(gj, 'w') as f:
                            json.dump(g, f, indent=2)
                updated += 1
            else:
                skipped += 1
        except Exception as e:
            errors += 1
            print(f"  ERROR {d}: {e}")

    action = "Would update" if check else "Updated"
    print(f"{action} {updated} ported gates, skipped {skipped}, not-gatable {not_gatable}, errors {errors}")


if __name__ == "__main__":
    check = "--check" in sys.argv
    fix_ported_gates(check=check)

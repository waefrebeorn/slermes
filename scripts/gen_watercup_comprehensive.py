#!/usr/bin/env python3
"""
gen_watercup_comprehensive.py — generate watercup gates for ALL 18k+ functions.

For every function in live_parity_scan.json:
  - PORTED: harness.c with extern to C port + twin.py calling LIVE Python
  - REAL_GAP: harness.c with GAP sentinel + twin.py calling LIVE Python (EXPECTED to mismatch)
  - PARTIAL: harness.c with extern to C port + twin.py (may mismatch)

The watercup pass rate = correct C ports / total functions.
When a gap is closed, its watercup flips from MISMATCH to MATCH.

Usage:
  python3 scripts/gen_watercup_comprehensive.py --scan     # scan for missing gates
  python3 scripts/gen_watercup_comprehensive.py --generate # generate all missing gates
  python3 scripts/gen_watercup_comprehensive.py --generate --limit 100
  python3 scripts/gen_watercup_comprehensive.py --regen-gap  # regenerate gap sentinels
"""
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_FILE = os.path.join(REPO, "live_parity_scan.json")
GATES = os.path.join(REPO, "tests", "watercup", "gates")
DEV = os.path.dirname(REPO)  # /home/wubu/hermes-agent-dev

# PoP annotation pattern
POP_RE = re.compile(r'/\*\s*PoP:\s*(\w+)\s*@([^:]+):(\w+)\s*\*/')


def load_scan():
    with open(SCAN_FILE) as f:
        return json.load(f)


_POP_INDEX_CACHE = None

def _load_pop_index():
    global _POP_INDEX_CACHE
    if _POP_INDEX_CACHE is None:
        index_path = os.path.join(REPO, "tests", "watercup", "pop_index.json")
        if os.path.isfile(index_path):
            _POP_INDEX_CACHE = json.load(open(index_path))
        else:
            _POP_INDEX_CACHE = {}
    return _POP_INDEX_CACHE


def find_c_port_for_function(py_module, py_fn):
    """Find the C port for a Python function via PoP index. Fast."""
    idx = _load_pop_index()
    key = f"{py_module}:{py_fn}"
    if key in idx:
        entry = idx[key]
        return entry["c_fn"], entry["c_file"]
    return None, None


def gate_dir_name(classification, py_module, py_fn, parent_class=""):
    """Generate a unique directory name for the gate."""
    mod = py_module.replace('/', '_').replace('.', '_').replace('-', '_')
    cls = classification.lower()[:4]  # port, real, part
    if parent_class:
        return f"{cls}__{mod}__{parent_class}__{py_fn}"
    return f"{cls}__{mod}__{py_fn}"


def generate_gap_sentinel_harness(gate_name, py_module, py_fn, ret_type="int"):
    """Generate a harness.c that returns a gap sentinel.
    This is for REAL_GAP functions that have no C port yet."""
    sentinel = "0" if ret_type == "int" else "false" if ret_type == "bool" else '""'

    return f"""/* gate: {gate_name} — GAP sentinel for {py_module}:{py_fn} */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static {ret_type} gap_sentinel(const char *input) {{
    (void)input;
    return {sentinel};
}}

static void trace_one(const char *input) {{
    WCUP_BEGIN("{py_fn}");
    {ret_type} result = gap_sentinel(input);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", "{py_fn}");
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\\n", cup);
    WCUP_END("{py_fn}");
}}

int main(int argc, char **argv) {{
    if (argc < 3) {{ fprintf(stderr, "usage: %s {py_fn} <input>...\\n", argv[0]); return 2; }}
    for (int i = 2; i < argc; i++) {{
        trace_one(argv[i]);
    }}
    return 0;
}}
"""


def generate_twin(py_module, py_fn, parent_class=""):
    """Generate twin.py for a function."""
    mod_under = py_module.replace('/', '_').replace('.', '_').replace('-', '_')

    # Build the call code as a string, then interpolate into the template
    if parent_class:
        setup_lines = [
            'try:',
            f'    _cls = getattr(_pb, "{parent_class}")',
            '    _obj = object.__new__(_cls)',
            'except Exception as e:',
            '    _obj = None',
        ]
        call_lines = [
            'if _obj is not None:',
            f'    result = _obj.{py_fn}(inp)',
            'else:',
            f'    raise RuntimeError("Cannot instantiate {parent_class}")',
        ]
        setup_str = '\n'.join(setup_lines)
        call_str = '\n'.join(call_lines)
    else:
        setup_str = ''
        call_str = f'result = _pb.{py_fn}(inp)'

    template = f'''"""twin for {py_module}:{py_fn}{" (class " + parent_class + ")" if parent_class else ""}."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_{mod_under}", os.path.join(DEV, "{py_module}"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_{mod_under}"] = _pb
_spec.loader.exec_module(_pb)

{setup_str}
def trace(inp):
    c = Cup("{py_fn}")
    try:
        {call_str}
        c.valstr(str(result))
    except Exception as e:
        c.valstr(f"ERROR:{{type(e).__name__}}")
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]
'''
    return template


def generate_gate_json(gate_name, classification, py_module, py_fn, inputs, c_file=None):
    """Generate gate.json for a gate."""
    gate = {
        "name": gate_name,
        "subsystem": "auto",
        "kind": "cup",
        "c_src": "harness.c",
        "c_fn": py_fn,
        "py_module": py_module,
        "py_fn": py_fn,
        "inputs": inputs,
        "extra_src": ["lib/libjson/json.c"],
        "cflags": ["-I", "include", "-I", "lib", "-I", "lib/libjson", "-I", "src"],
        "classification": classification,
    }
    if c_file:
        rel = os.path.relpath(c_file, REPO)
        gate["extra_src"].append(rel)
    return gate


def generate_port_harness(gate_name, c_fn, c_file, py_module, py_fn, ret_type="int", params="const char *input"):
    """Generate harness.c for a PORTED function that calls the real C port."""
    if ret_type == "int":
        trace_body = f"""    int result = {c_fn}(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);"""
    elif ret_type == "bool":
        trace_body = f"""    bool result = {c_fn}(input);
    WCUP_VALSTR(result ? "true" : "false");"""
    else:
        trace_body = f"""    {ret_type} result = {c_fn}(input);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", str(result));
    WCUP_VALSTR(buf);"""

    return f"""/* gate: {gate_name} — traced copy of {c_fn} */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern {ret_type} {c_fn}({params});

static void trace_one(const char *input) {{
    WCUP_BEGIN("{c_fn}");
{trace_body}
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\\n", cup);
    WCUP_END("{c_fn}");
}}

int main(int argc, char **argv) {{
    if (argc < 3) {{ fprintf(stderr, "usage: %s {c_fn} <input>...\\n", argv[0]); return 2; }}
    for (int i = 2; i < argc; i++) {{
        trace_one(argv[i]);
    }}
    return 0;
}}
"""


def generate_missing_gates(limit=0):
    """Generate all missing watercup gates."""
    scan = load_scan()
    mods = scan.get("modules", {})

    generated = 0
    skipped = 0
    errors = 0

    for key, info in mods.items():
        for g in info.get("gaps", []):
            if limit and generated >= limit:
                print(f"Generated {generated} gates (limit {limit}), skipped {skipped}, errors {errors}")
                return

            feat = g.get("python_feature", {})
            classification = g.get("classification", "?")
            py_module = feat.get("module", key)
            py_fn = feat.get("name", "?")
            kind = feat.get("kind", "fn")

            gate_name = gate_dir_name(classification, py_module, py_fn)
            gdir = os.path.join(GATES, gate_name)

            # Skip if gate already exists
            if os.path.isfile(os.path.join(gdir, "gate.json")):
                skipped += 1
                continue

            try:
                os.makedirs(gdir, exist_ok=True)

                inputs = ["hello", "world", "", "test123", "NULL"]

                if classification == "PORTED":
                    # Ported function — find C port and generate real harness
                    c_fn, c_file = find_c_port_for_function(py_module, py_fn)
                    if c_fn:
                        harness = generate_port_harness(gate_name, c_fn, c_file, py_module, py_fn)
                        gate_json = generate_gate_json(gate_name, classification, py_module, py_fn, inputs, c_file)
                    else:
                        # Ported but no PoP annotation found — generate gap sentinel as fallback
                        harness = generate_gap_sentinel_harness(gate_name, py_module, py_fn)
                        gate_json = generate_gate_json(gate_name, classification, py_module, py_fn, inputs)
                elif classification == "REAL_GAP":
                    # Real gap — generate sentinel harness
                    harness = generate_gap_sentinel_harness(gate_name, py_module, py_fn)
                    gate_json = generate_gate_json(gate_name, classification, py_module, py_fn, inputs)
                elif classification == "PARTIAL":
                    # Partial — try to find C port, fall back to gap sentinel
                    c_fn, c_file = find_c_port_for_function(py_module, py_fn)
                    if c_fn:
                        harness = generate_port_harness(gate_name, c_fn, c_file, py_module, py_fn)
                        gate_json = generate_gate_json(gate_name, classification, py_module, py_fn, inputs, c_file)
                    else:
                        harness = generate_gap_sentinel_harness(gate_name, py_module, py_fn)
                        gate_json = generate_gate_json(gate_name, classification, py_module, py_fn, inputs)
                else:
                    continue

                with open(os.path.join(gdir, "harness.c"), 'w') as f:
                    f.write(harness)

                twin = generate_twin(py_module, py_fn)
                with open(os.path.join(gdir, "twin.py"), 'w') as f:
                    f.write(twin)

                with open(os.path.join(gdir, "gate.json"), 'w') as f:
                    json.dump(gate_json, f, indent=2)

                generated += 1

            except Exception as e:
                errors += 1
                print(f"ERROR {gate_name}: {e}")

    print(f"Generated {generated} gates, skipped {skipped} (exist), errors {errors}")


def scan_missing():
    """Scan for how many gates are missing."""
    scan = load_scan()
    mods = scan.get("modules", {})

    total_funcs = 0
    existing_gates = 0
    missing_gates = 0

    existing_names = set(os.listdir(GATES))

    for key, info in mods.items():
        for g in info.get("gaps", []):
            feat = g.get("python_feature", {})
            classification = g.get("classification", "?")
            py_module = feat.get("module", key)
            py_fn = feat.get("name", "?")

            gate_name = gate_dir_name(classification, py_module, py_fn)
            total_funcs += 1

            if gate_name in existing_names and os.path.isfile(os.path.join(GATES, gate_name, "gate.json")):
                existing_gates += 1
            else:
                missing_gates += 1

    print(f"Total functions in scan: {total_funcs}")
    print(f"Existing gates: {existing_gates}")
    print(f"Missing gates: {missing_gates}")
    print(f"Coverage: {existing_gates}/{total_funcs} ({100.0*existing_gates/total_funcs:.1f}%)")
    return missing_gates


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Comprehensive watercup gate generation")
    parser.add_argument('--scan', action='store_true', help='Scan for missing gates')
    parser.add_argument('--generate', action='store_true', help='Generate missing gates')
    parser.add_argument('--limit', type=int, default=0, help='Limit number of gates')
    parser.add_argument('--regen-gap', action='store_true', help='Regenerate gap sentinel gates')
    args = parser.parse_args()

    if args.generate:
        generate_missing_gates(limit=args.limit)
    elif args.regen_gap:
        # TODO: regenerate gap sentinels
        pass
    else:
        scan_missing()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
gen_watercup.py — auto-generate watercup gates for pure functions.

For each pure function (int fn(const char*), bool fn(const char*), etc.):
  - Generate harness.c (traced C copy)
  - Generate twin.py (traced Python twin)
  - Generate gate.json (gate manifest)

Usage:
  python3 scripts/gen_watercup.py --scan          # scan and report counts
  python3 scripts/gen_watercup.py --generate      # generate all missing gates
  python3 scripts/gen_watercup.py --generate --limit 100  # generate first 100
"""

import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "src")
GATES = os.path.join(REPO, "tests", "watercup", "gates")

POP_PATTERN = re.compile(r'/\*\s*PoP:\s*(\w+)\s*@\s*([^:]+):(\w+)\s*\*/')


def find_c_file(c_fn):
    """Find which C file contains the definition of c_fn."""
    for root, dirs, files in os.walk(SRC):
        for f in files:
            if f.endswith('.c'):
                path = os.path.join(root, f)
                with open(path) as fh:
                    content = fh.read()
                # Look for function definition (not just call)
                if re.search(rf'(?:int|bool|char|long|double|size_t|void)\s+{re.escape(c_fn)}\s*\(', content):
                    return path
    return None


def scan_ports():
    """Scan all C files for PoP annotations and categorize by signature."""
    ports = {
        'int_str': [],      # int fn(const char*)
        'bool_str': [],     # bool fn(const char*)
        'int_str_str': [],  # int fn(const char*, const char*)
        'bool_str_str': [], # bool fn(const char*, const char*)
        'int_int': [],      # int fn(int)
        'bool_int': [],     # bool fn(int)
        'other': [],
    }

    for root, dirs, files in os.walk(SRC):
        for f in files:
            if f.endswith('.c'):
                path = os.path.join(root, f)
                with open(path) as fh:
                    content = fh.read()
                for m in POP_PATTERN.finditer(content):
                    c_fn = m.group(1)
                    py_module = m.group(2)
                    py_fn = m.group(3)

                    pos = m.start()
                    rest = content[pos:pos+1000]

                    sig_match = re.search(
                        r'(?:static\s+)?(?:const\s+)?(?:unsigned\s+)?(?:long\s+)?(?:struct\s+)?(\w+)\s+' + re.escape(c_fn) + r'\s*\(([^)]*)\)',
                        rest
                    )
                    if not sig_match:
                        ports['other'].append((c_fn, py_module, py_fn, path))
                        continue

                    ret_type = sig_match.group(1)
                    params = sig_match.group(2).strip()
                    param_list = [p.strip() for p in params.split(',') if p.strip()]

                    entry = {
                        'c_fn': c_fn,
                        'py_module': py_module,
                        'py_fn': py_fn,
                        'c_file': path,
                        'ret_type': ret_type,
                        'params': params,
                    }

                    if ret_type == 'int' and len(param_list) == 1 and 'char' in param_list[0]:
                        ports['int_str'].append(entry)
                    elif ret_type == 'bool' and len(param_list) == 1 and 'char' in param_list[0]:
                        ports['bool_str'].append(entry)
                    elif ret_type == 'int' and len(param_list) == 2 and all('char' in p for p in param_list):
                        ports['int_str_str'].append(entry)
                    elif ret_type == 'bool' and len(param_list) == 2 and all('char' in p for p in param_list):
                        ports['bool_str_str'].append(entry)
                    elif ret_type == 'int' and len(param_list) == 1 and 'int' in param_list[0]:
                        ports['int_int'].append(entry)
                    elif ret_type == 'bool' and len(param_list) == 1 and 'int' in param_list[0]:
                        ports['bool_int'].append(entry)
                    else:
                        ports['other'].append(entry)

    return ports


def gate_name(c_fn, py_module, py_fn):
    """Generate a unique gate name."""
    mod = py_module.replace('/', '_').replace('.', '_')
    return f"{mod}__{py_fn}"


def generate_int_str_harness(entry):
    """Generate harness.c for int fn(const char*)."""
    c_fn = entry['c_fn']
    name = gate_name(c_fn, entry['py_module'], entry['py_fn'])
    lines = [
        f"/* gate: {name} — traced copy of {c_fn} */",
        "#include <stdio.h>",
        "#include <stdlib.h>",
        "#include <string.h>",
        '#include "watercup.h"',
        "",
        f"extern int {c_fn}(const char *input);",
        "",
        "static void trace_one(const char *input) {",
        f'    WCUP_BEGIN("{c_fn}");',
        f"    int result = {c_fn}(input);",
        "    char buf[32];",
        '    snprintf(buf, sizeof(buf), "%d", result);',
        "    WCUP_VALSTR(buf);",
        "    char cup[32];",
        "    WCUP_FINALIZE(cup, sizeof cup);",
        '    printf("%s\\n", cup);',
        f'    WCUP_END("{c_fn}");',
        "}",
        "",
        "int main(int argc, char **argv) {",
        f'    if (argc < 2) {{ fprintf(stderr, "usage: %s {c_fn} <input>...\\n", argv[0]); return 2; }}',
        "    for (int i = 1; i < argc; i++) {",
        "        trace_one(argv[i]);",
        "    }",
        "    return 0;",
        "}",
    ]
    return "\n".join(lines) + "\n"


def generate_bool_str_harness(entry):
    """Generate harness.c for bool fn(const char*)."""
    c_fn = entry['c_fn']
    name = gate_name(c_fn, entry['py_module'], entry['py_fn'])
    lines = [
        f"/* gate: {name} — traced copy of {c_fn} */",
        "#include <stdio.h>",
        "#include <stdlib.h>",
        "#include <string.h>",
        "#include <stdbool.h>",
        '#include "watercup.h"',
        "",
        f"extern bool {c_fn}(const char *input);",
        "",
        "static void trace_one(const char *input) {",
        f'    WCUP_BEGIN("{c_fn}");',
        f"    bool result = {c_fn}(input);",
        '    WCUP_VALSTR(result ? "true" : "false");',
        "    char cup[32];",
        "    WCUP_FINALIZE(cup, sizeof cup);",
        '    printf("%s\\n", cup);',
        f'    WCUP_END("{c_fn}");',
        "}",
        "",
        "int main(int argc, char **argv) {",
        f'    if (argc < 2) {{ fprintf(stderr, "usage: %s {c_fn} <input>...\\n", argv[0]); return 2; }}',
        "    for (int i = 1; i < argc; i++) {",
        "        trace_one(argv[i]);",
        "    }",
        "    return 0;",
        "}",
    ]
    return "\n".join(lines) + "\n"


def generate_int_str_str_harness(entry):
    """Generate harness.c for int fn(const char*, const char*)."""
    c_fn = entry['c_fn']
    name = gate_name(c_fn, entry['py_module'], entry['py_fn'])
    lines = [
        f"/* gate: {name} — traced copy of {c_fn} */",
        "#include <stdio.h>",
        "#include <stdlib.h>",
        "#include <string.h>",
        '#include "watercup.h"',
        "",
        f"extern int {c_fn}(const char *a, const char *b);",
        "",
        "static void trace_one(const char *a, const char *b) {",
        f'    WCUP_BEGIN("{c_fn}");',
        f"    int result = {c_fn}(a, b);",
        "    char buf[32];",
        '    snprintf(buf, sizeof(buf), "%d", result);',
        "    WCUP_VALSTR(buf);",
        "    char cup[32];",
        "    WCUP_FINALIZE(cup, sizeof cup);",
        '    printf("%s\\n", cup);',
        f'    WCUP_END("{c_fn}");',
        "}",
        "",
        "int main(int argc, char **argv) {",
        f'    if (argc < 3) {{ fprintf(stderr, "usage: %s {c_fn} <a> <b>\\n", argv[0]); return 2; }}',
        "    trace_one(argv[1], argv[2]);",
        "    return 0;",
        "}",
    ]
    return "\n".join(lines) + "\n"


def generate_twin(entry):
    """Generate twin.py for any function type."""
    py_module = entry['py_module']
    py_fn = entry['py_fn']
    c_fn = entry['c_fn']
    ret_type = entry['ret_type']
    params = entry['params']
    param_list = [p.strip() for p in params.split(',') if p.strip()]

    if len(param_list) == 1:
        call_args = "inp"
        trace_call = f"_pb.{py_fn}(inp)"
    elif len(param_list) == 2:
        call_args = "a, b"
        trace_call = f"_pb.{py_fn}(a, b)"
    else:
        call_args = "inp"
        trace_call = f"_pb.{py_fn}(inp)"

    if ret_type == 'int':
        val_event = "c.valstr(str(result))"
    elif ret_type == 'bool':
        val_event = 'c.valstr("true" if result else "false")'
    else:
        val_event = "c.valstr(str(result))"

    mod_under = py_module.replace('/', '_').replace('.', '_')

    lines = [
        f'"""twin for {gate_name(c_fn, py_module, py_fn)}."""',
        "import os, sys, importlib.util as ilu",
        "",
        "HERE = os.path.dirname(os.path.abspath(__file__))",
        "SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))",
        "DEV = os.path.dirname(SLERMES)",
        "sys.path.insert(0, DEV)",
        "",
        "from watercup_py import Cup",
        "",
        f'_spec = ilu.spec_from_file_location(',
        f'    "wc_live_{mod_under}", os.path.join(DEV, "{py_module}"))',
        f"_pb = ilu.module_from_spec(_spec)",
        f'sys.modules["wc_live_{mod_under}"] = _pb',
        "_spec.loader.exec_module(_pb)",
        "",
        "",
        f"def trace({call_args}):",
        f'    c = Cup("{c_fn}")',
        f"    result = {trace_call}",
        f"    {val_event}",
        "    return c.finalize()",
        "",
        "",
        "def run(inputs):",
        "    return [trace(inp) for inp in inputs]",
    ]
    return "\n".join(lines) + "\n"


def generate_gate_json(entry, inputs):
    """Generate gate.json for a function."""
    c_fn = entry['c_fn']
    py_module = entry['py_module']
    py_fn = entry['py_fn']
    c_file = entry.get('c_file')

    extra_src = ["lib/libjson/json.c"]
    if c_file:
        rel = os.path.relpath(c_file, REPO)
        extra_src.append(rel)

    gate = {
        "name": gate_name(c_fn, py_module, py_fn),
        "subsystem": "auto",
        "kind": "cup",
        "c_src": "harness.c",
        "c_fn": c_fn,
        "py_module": py_module,
        "py_fn": py_fn,
        "inputs": inputs,
        "extra_src": extra_src,
        "cflags": ["-I", "include", "-I", "lib", "-I", "lib/libjson", "-I", "src"],
    }

    return gate


def get_default_inputs(entry):
    """Get default test inputs based on function signature."""
    ret_type = entry['ret_type']
    params = entry['params']
    param_list = [p.strip() for p in params.split(',') if p.strip()]

    if len(param_list) == 1:
        if 'char' in param_list[0]:
            return ["hello", "world", "", "test123", "NULL"]
        elif 'int' in param_list[0]:
            return ["0", "1", "-1", "42", "999"]
    elif len(param_list) == 2:
        if all('char' in p for p in param_list):
            return ["hello,world", "foo,bar", "a,b", "test,test", "NULL,NULL"]

    return ["test"]


def generate_gate(entry, gate_dir):
    """Generate all files for a single gate."""
    c_fn = entry['c_fn']
    py_module = entry['py_module']
    py_fn = entry['py_fn']
    ret_type = entry['ret_type']
    params = entry['params']
    param_list = [p.strip() for p in params.split(',') if p.strip()]

    name = gate_name(c_fn, py_module, py_fn)
    gdir = os.path.join(gate_dir, name)

    if os.path.exists(os.path.join(gdir, "gate.json")):
        return False

    os.makedirs(gdir, exist_ok=True)

    inputs = get_default_inputs(entry)

    if ret_type == 'int' and len(param_list) == 1 and 'char' in param_list[0]:
        harness = generate_int_str_harness(entry)
    elif ret_type == 'bool' and len(param_list) == 1 and 'char' in param_list[0]:
        harness = generate_bool_str_harness(entry)
    elif ret_type == 'int' and len(param_list) == 2 and all('char' in p for p in param_list):
        harness = generate_int_str_str_harness(entry)
    else:
        harness = generate_int_str_harness(entry)

    with open(os.path.join(gdir, "harness.c"), 'w') as f:
        f.write(harness)

    twin = generate_twin(entry)
    with open(os.path.join(gdir, "twin.py"), 'w') as f:
        f.write(twin)

    gate_json = generate_gate_json(entry, inputs)
    with open(os.path.join(gdir, "gate.json"), 'w') as f:
        json.dump(gate_json, f, indent=2)

    return True


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Auto-generate watercup gates")
    parser.add_argument('--scan', action='store_true', help='Scan and report counts')
    parser.add_argument('--generate', action='store_true', help='Generate missing gates')
    parser.add_argument('--limit', type=int, default=0, help='Limit number of gates to generate')
    args = parser.parse_args()

    ports = scan_ports()

    if args.scan or not args.generate:
        print("Watercup auto-gateability scan:")
        for cat, fns in sorted(ports.items(), key=lambda x: -len(x[1])):
            print(f"  {cat}: {len(fns)}")
        total = sum(len(fns) for fns in ports.values())
        print(f"\nTotal: {total}")
        return

    if args.generate:
        pure_cats = ['int_str', 'bool_str', 'int_str_str', 'bool_str_str', 'int_int', 'bool_int']
        generated = 0
        skipped = 0

        for cat in pure_cats:
            for entry in ports[cat]:
                if args.limit and generated >= args.limit:
                    print(f"Generated {generated} gates (limit {args.limit})")
                    return

                if generate_gate(entry, GATES):
                    generated += 1
                else:
                    skipped += 1

        print(f"Generated {generated} gates, skipped {skipped} (already exist)")


if __name__ == '__main__':
    main()

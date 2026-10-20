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


try:
    from gen_watercup_twin_rich import (_c_ret_enc_expr, _c_out_specs,
                                        _twin_needs_conv, generate_twin_rich)
except ImportError:
    # Imported from a context where scripts/ is not on sys.path (e.g. a
    # runner that added only this file's dir): load the sibling by path.
    import importlib.util as _ilu
    import os as _os
    _rp = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)),
                        "gen_watercup_twin_rich.py")
    _rs = _ilu.spec_from_file_location("gen_watercup_twin_rich", _rp)
    _rm = _ilu.module_from_spec(_rs)
    _rs.loader.exec_module(_rm)
    _c_ret_enc_expr = _rm._c_ret_enc_expr
    _c_out_specs = _rm._c_out_specs
    _twin_needs_conv = _rm._twin_needs_conv
    generate_twin_rich = _rm.generate_twin_rich



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


def generate_twin(py_module, py_fn, parent_class="", c_ret=None,
                  cup_name=None, c_params=None):
    """Generate twin.py for a function.

    c_ret is the C return type of the ported function ("int", "bool",
    "charptr", "void", "double", "long", "size_t", "json"). The twin must
    emit the SAME canonical string the C harness feeds WCUP_VALSTR, or
    every cup mismatches for encoding reasons rather than logic ones.

    cup_name is the C symbol the harness passes to WCUP_BEGIN. Cup() folds
    that name into the hash before any value, so a twin that opens its cup
    with the PYTHON name can never match a harness that opened with the C
    name — even when every traced value is identical. Default to cup_name.

    c_params is the port's C param list. When it carries scalar out-params
    or char* buffer-outs the twin traces the SAME out values the harness
    traces (the scalar_outparam harness ignores the C return value); when
    it carries json_t*/scalar-by-value inputs the twin converts fixture
    strings exactly like the harness does (json.loads / strtol / strtod
    emulation). Without specs or conversion needs the legacy template is
    emitted byte-identically.
    """
    specs = _c_out_specs(c_params)
    if specs or _twin_needs_conv(c_params):
        return generate_twin_rich(py_module, py_fn, parent_class, c_ret,
                                 cup_name, specs)
    mod_under = py_module.replace('/', '_').replace('.', '_').replace('-', '_')
    cup_name = cup_name or py_fn

    # Canonical encoding mirroring fix_all_gates.generate_port_harness.
    # When the C function returns int, a Python bool must be encoded as its
    # integer value (int(True) -> "1"), NOT as "True"/"False" — C emits "%d".
    # The isinstance-bool exclusion here was backwards and made every
    # bool-returning twin mismatch on encoding alone.
    enc = _c_ret_enc_expr(c_ret)

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
        # Arity is introspected at trace time rather than assuming one
        # positional arg: a zero-arg or multi-arg function raises TypeError on
        # every input, so the twin cup collapses to a constant ERROR and the
        # gate measures nothing.
        call_str = 'result = _call(inp, inputs)'

    helper = '''
def _resolve_fn():
    """Module-level fn, else a method on any class in the LIVE module."""
    f = getattr(_pb, "%s", None)
    if f is not None:
        return f
    import inspect as _i
    for _n, _o in vars(_pb).items():
        if _i.isclass(_o) and hasattr(_o, "%s"):
            _m = getattr(_o, "%s")
            if callable(_m):
                return _m.__get__(_DummySelf(), _o)
    return None


class _DummySelf:
    """Permissive stand-in for self: any attribute yields another _DummySelf,
    so pure methods run. Methods that need real instance state raise and are
    reported as an ERROR cup — honest, not a false MATCH."""
    def __getattr__(self, name):
        return _DummySelf()
    def __call__(self, *a, **k):
        return _DummySelf()


_FN = _resolve_fn()


def _call(inp, inputs, idx=0):
    """Call the LIVE twin with as many inputs as its signature accepts.

    Parameter 0 receives the current input (inp). Parameter j>0 receives the
    walk position idx+j (wrapping) — the SAME position the C harness feeds it
    via argv[2 + (i - 2 + j) %% n]. Using a fixed position for j>0 made a
    2-arg function compute f(x, inputs[1]) on every step, collapsing the cup.

    A param annotated Path receives Path(inp) — the raw fixture is a str, and
    file helpers call .stat()/.read_text() on it, which raises AttributeError
    on a str and folds every cup to a constant ERROR. Wrapping gives the twin
    the Path object the signature promises; the C side still sees the path
    string via argv.
    """
    import inspect as _insp
    from pathlib import Path as _Path
    try:
        _sig = _insp.signature(_FN)
    except Exception:
        _sig = None
    if _sig is not None:
        _pos = [p for p in _sig.parameters.values()
                if p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD)]
        _np = len(_pos)
        _wrap = _pos and "Path" in str(_pos[0].annotation)
        _a0 = _Path(inp) if (_wrap and isinstance(inp, str)) else inp
        if _np <= 1:
            return _FN(_a0) if _np else _FN()
        n = len(inputs)
        return _FN(_a0, *[inputs[(idx + j) %% n] for j in range(1, _np)])
    try:
        return _FN(inp)
    except TypeError:
        return _FN()
''' % (py_fn, py_fn, py_fn)

    template = f'''"""twin for {py_module}:{py_fn}{" (class " + parent_class + ")" if parent_class else ""}."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

# Relative imports (from .config import ...) need the module loaded inside its
# real package. spec_from_file_location with a bare synthetic name has no
# package context -> "attempted relative import with no known parent package".
# Register each ancestor package (pointing at its dir on sys.path) and give the
# module a dotted name so '.' resolves to the parent package.
_pkg_parts = "{py_module}".replace(".py", "").split("/")
_mod_name = ".".join(_pkg_parts)
for _i in range(1, len(_pkg_parts)):
    _anc = ".".join(_pkg_parts[:_i])
    if _anc in sys.modules:
        continue
    # Prefer the REAL package (runs its __init__.py, so names it exports are
    # available). Only synthesize a namespace-style ModuleType when the real
    # import fails -- a synthetic one bypasses __init__.py and breaks
    # 'from pkg import name' for names defined there.
    try:
        __import__(_anc)
    except Exception:
        import types as _t
        _m = _t.ModuleType(_anc)
        _m.__path__ = [os.path.join(DEV, *_pkg_parts[:_i])]
        _m.__package__ = _anc
        sys.modules[_anc] = _m
_spec = ilu.spec_from_file_location(_mod_name, os.path.join(DEV, "{py_module}"))
_pb = ilu.module_from_spec(_spec)
_pb.__package__ = ".".join(_pkg_parts[:-1])
sys.modules[_mod_name] = _pb
_spec.loader.exec_module(_pb)

{setup_str}{helper}
def trace(inp, inputs, idx=0):
    c = Cup("{cup_name}")
    try:
        result = _call(inp, inputs, idx)
        c.valstr({enc})
    except Exception as e:
        c.valstr(f"ERROR:{{type(e).__name__}}")
    return c.finalize()


def run(inputs):
    return [trace(inp, inputs, i) for i, inp in enumerate(inputs)]
'''
    return template


def generate_inputs_for_signature(py_fn):
    """Pick fixture values that match the function's first parameter type.

    One generic string walk was used for every gate. A function whose first
    parameter is an int (e.g. _bytes_to_tokens(n: int)) raises ValueError on
    "hello", so the twin folds every input to the same ERROR cup — a constant
    that can never match a varying C cup, and never measures the port. Emit
    typed fixtures instead so the walk actually exercises the logic.
    """
    import importlib
    mod = None
    # _pb is not available here; the caller passes only the fn name, so infer
    # from the annotation on the LIVE module when importable.
    return None


def _typed_inputs(annotation):
    """Fixture walk for a parameter annotation string."""
    a = (annotation or "").lower()
    if "path" in a:
        # A Path param expects a real filesystem path. The twin calls
        # .stat()/.read_text()/open() on it; the C port opens the same path
        # via argv. Create real temp files with varied content so BOTH sides
        # operate on identical inputs and the cup reflects real logic, not a
        # constant AttributeError from passing a bare str.
        import tempfile
        import os as _os
        contents = ["line1\nline2\nline3\n", "", "single\n",
                    "a\nb\nc\nd\ne\n", "no-newline-at-eof"]
        paths = []
        for i, c in enumerate(contents):
            fd, p = tempfile.mkstemp(prefix="wcup_p%d_" % i, suffix=".txt")
            with _os.fdopen(fd, "w") as f:
                f.write(c)
            paths.append(p)
        return paths
    if "int" in a and "point" not in a:
        return [0, 1, 42, 1024, -7]
    if "float" in a or "double" in a:
        return [0.0, 1.5, 3.14159, 1024.0, -2.5]
    if "bool" in a:
        return [True, False]
    # Containers cannot cross the argv boundary: run_harness passes
    # str(input), so a dict fixture arrives at BOTH sides as the literal
    # string "{'a': 1}" and the twin raises AttributeError on .get(). Fall
    # back to the scalar walk rather than emit an unusable fixture.
    return ["hello", "world", "", "test123", "NULL"]


def _resolve_callable(mod, py_fn):
    """Find the LIVE callable for py_fn: module-level, else a method on any
    class in the module. Many ported helpers are class methods
    (BrowserProvider.display_name); getattr(mod, fn) returns None for those
    and the twin folds every input to ERROR:AttributeError."""
    fn = getattr(mod, py_fn, None)
    if fn is not None:
        return fn
    import inspect as _insp
    for _name, obj in vars(mod).items():
        if inspect_is_class(obj) and hasattr(obj, py_fn):
            m = getattr(obj, py_fn)
            if callable(m):
                return _UnboundMethod(m)
    return None


def inspect_is_class(obj):
    import inspect
    return inspect.isclass(obj)


class _UnboundMethod:
    """Wraps a class method so it can be called without an instance.

    Binds `self` to a dummy: methods that only use other params (pure
    formatting/validation) work; ones that read real instance state raise
    and are reported as an ERROR cup — honest, not a false MATCH.
    """

    def __init__(self, func):
        self._func = func

    def __call__(self, *args, **kwargs):
        import inspect
        try:
            sig = inspect.signature(self._func)
            if 'self' in sig.parameters:
                return self._func(_DummySelf(), *args, **kwargs)
        except Exception:
            pass
        return self._func(*args, **kwargs)


class _DummySelf:
    """Permissive stand-in for `self` — returns a new _DummySelf for any
    attribute so pure methods run, and record access is visible."""

    def __getattr__(self, name):
        return _DummySelf()

    def __call__(self, *a, **k):
        return _DummySelf()


def _fixtures_for(py_module, py_fn):
    """Typed fixture walk for a LIVE function, from its first parameter's
    annotation. Falls back to the generic string walk when the module cannot
    be imported or the parameter is unannotated."""
    import os as _os
    import sys as _sys
    import importlib.util as _ilu
    # This file lives at <repo>/scripts/, and the LIVE Python tree is the
    # parent repo: <dev>/agent/... So dev is THREE levels up, not four.
    dev = _os.path.dirname(_os.path.dirname(_os.path.dirname(
        _os.path.abspath(__file__))))
    path = _os.path.join(dev, py_module)
    if not _os.path.isfile(path):
        return _typed_inputs(None)
    try:
        if dev not in _sys.path:
            _sys.path.insert(0, dev)
        modname = ("wc_fix_" + py_module.replace("/", "_")
                   .replace(".", "_"))
        spec = _ilu.spec_from_file_location(modname, path)
        mod = _ilu.module_from_spec(spec)
        # Register BEFORE exec: modules that use dataclasses, enums, or
        # relative imports resolve their own name through sys.modules during
        # execution. Skipping this raises "'NoneType' object has no attribute
        # '__dict__'" and the fixture silently falls back to the string walk.
        _sys.modules[modname] = mod
        spec.loader.exec_module(mod)
        fn = _resolve_callable(mod, py_fn)
        if fn is None:
            return _typed_inputs(None)
        import inspect as _insp
        sig = _insp.signature(fn)
        for p in sig.parameters.values():
            if p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD):
                return _typed_inputs(str(p.annotation))
        return _typed_inputs(None)
    except Exception as _e:
        if os.environ.get("WC_DEBUG_FIXTURES"):
            import sys as _s
            _s.stderr.write("DBG fixtures %s:%s -> %s: %s\n"
                            % (py_module, py_fn, type(_e).__name__, _e))
        return _typed_inputs(None)


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

                inputs = _fixtures_for(py_module, py_fn)

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

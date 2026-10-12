#!/usr/bin/env python3
"""
fix_all_gates.py — regenerate ALL watercup gates with correct C harnesses.

Ported gates → call real C function via extern + link
GAP gates → gap sentinel (tracks existence, flips to MATCH when ported)
Legacy → leave if passing

Usage: python3 tests/watercup/fix_all_gates.py [--check]
"""
import json
import os
import re
import sys

REPO = os.getcwd()
GATES = os.path.join(REPO, "tests/watercup", "gates")
SRC = os.path.join(REPO, "src")
POP_INDEX_PATH = os.path.join(REPO, "tests/watercup", "pop_index.json")

# Leaf utilities nearly every ported unit calls: logging and path resolution.
# Both are self-contained (libc + pthread only) and already standalone build
# objects, so linking them costs nothing and removes the two most common
# undefined-reference failures (hermes_log, auth_file_path) that would
# otherwise be reported as cup divergences.
COMMON_SRC = [
    "lib/libjson/json.c",
    "src/agent/logger.c",
    "src/cli/paths.c",
]

SIG_RE = re.compile(
    r'(?:static\s+)?(?:const\s+)?(?:unsigned\s+)?(?:long\s+)?(?:struct\s+\w+\s+)?'
    r'(\w+)\s+(\w+)\s*\(([^)]*)\)\s*\{'
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


def categorize_signature(ret_type, param_list):
    unhandleable = ('json', 'struct', 'callback', 'void*', 'fn', 'va_list')
    # A bare `void` parameter means NO parameters. Keeping it in the list made
    # every zero-arg function fall through the char/int param tests and get
    # rejected as "not gatable" — e.g. bool _oauth_trace_enabled(void).
    if [p.strip() for p in param_list] == ['void']:
        param_list = []
    # A pointer to an opaque/named struct type (coe_engine *eng) is not a
    # string or a number: it cannot be built from argv, and emitting an
    # extern decl with an unknown type makes the harness fail to COMPILE.
    # Only char*/int/double/size_t params are harnessable.
    for p in param_list:
        if '*' in p:
            head = p.split('*')[0].strip()
            if head and 'char' not in head:
                return None
        for u in unhandleable:
            if u in p:
                return None
    if any(u in ret_type for u in unhandleable):
        return None
    if not param_list:
        if ret_type == 'void':
            return 'void'
        if ret_type == 'int':
            return 'int_noargs'
        if ret_type == 'bool':
            return 'bool_noargs'
        if ret_type == 'char':
            return 'charptr_noargs'
    if ret_type == 'void':
        return 'void'
    elif ret_type == 'int':
        if len(param_list) == 1 and 'char' in param_list[0]:
            return 'int_str'
        elif len(param_list) == 2 and all('char' in p for p in param_list):
            return 'int_str_str'
        elif len(param_list) == 1 and 'int' in param_list[0]:
            return 'int_int'
        elif len(param_list) <= 4 and all('char' in p or 'int' in p for p in param_list):
            return 'int_other'
        return None
    elif ret_type == 'bool':
        if len(param_list) == 1 and 'char' in param_list[0]:
            return 'bool_str'
        elif len(param_list) == 2 and all('char' in p for p in param_list):
            return 'bool_str_str'
        elif len(param_list) == 1 and 'int' in param_list[0]:
            return 'bool_int'
        elif len(param_list) <= 4 and all('char' in p or 'int' in p for p in param_list):
            return 'bool_other'
        return None
    elif ret_type == 'char':
        if len(param_list) <= 2 and all('char' in p or 'int' in p for p in param_list):
            return 'charptr_simple'
        return None
    elif ret_type == 'size_t':
        if len(param_list) <= 2 and all('char' in p or 'int' in p for p in param_list):
            return 'sizet_simple'
        return None
    elif ret_type == 'long':
        if len(param_list) <= 2 and all('char' in p or 'int' in p for p in param_list):
            return 'long_simple'
        return None
    elif ret_type == 'double':
        if len(param_list) <= 2 and all('double' in p or 'int' in p for p in param_list):
            return 'double_simple'
        return None
    return None


def generate_port_harness(category, gate_name, c_fn, params, param_list, ret_type,
                          inputs_walk=None):
    # Zero-arg categories pass nothing to the traced function; the input walk
    # still varies the cup via the CUP name + the traced call site.
    noargs = category.endswith('_noargs')
    # Categories whose params are ints/doubles must convert the string argv:
    # passing argv[i] straight to an `int` param is a compile error (and
    # silently wrong if it weren't).
    numeric = category in ('int_int', 'bool_int', 'int_other', 'bool_other',
                           'long_simple', 'sizet_simple', 'double_simple')
    # Emit the EXPRESSION for parameter j's argument. `i` is the C loop
    # variable over the INPUT range (argv[2] .. argv[argc-1]); parameters
    # beyond the first take the following walk positions, WRAPPING INSIDE the
    # input range. The wrap must be relative to the input base (argv[2]), not
    # absolute argv: `(i + j) % n` on an absolute i walks off the inputs and
    # reads argv[0]/argv[1] (program name, gate name) as data.
    n_avail = len(inputs_walk) if inputs_walk else 1

    def _arg(j):
        if n_avail <= 1 or j == 0:
            return 'argv[i]'
        return 'argv[2 + (i - 2 + %d) %% %d]' % (j, n_avail)

    if noargs:
        call_args = ''
    elif numeric:
        conv = ('(int)strtol(%s, NULL, 10)' if 'double' not in category
                else 'strtod(%s, NULL)')
        call_args = ', '.join(conv % _arg(j) for j in range(len(param_list)))
    else:
        call_args = ', '.join(_arg(j) for j in range(len(param_list)))

    if category.startswith('int'):
        val_code = '    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);'
    elif category.startswith('bool'):
        val_code = '    WCUP_VALSTR(result ? "true" : "false");'
    elif category == 'charptr_simple':
        val_code = '    WCUP_VALSTR(result ? result : "");'
    elif category == 'sizet_simple':
        val_code = '    char buf[32];\n    snprintf(buf, sizeof(buf), "%zu", result);\n    WCUP_VALSTR(buf);'
    elif category == 'long_simple':
        val_code = '    char buf[32];\n    snprintf(buf, sizeof(buf), "%ld", result);\n    WCUP_VALSTR(buf);'
    elif category == 'double_simple':
        val_code = '    char buf[64];\n    snprintf(buf, sizeof(buf), "%.17g", result);\n    WCUP_VALSTR(buf);'
    elif category == 'void':
        val_code = '    WCUP_VALSTR("void");'
    else:
        val_code = '    char buf[64];\n    snprintf(buf, sizeof(buf), "%p", (void*)&result);\n    WCUP_VALSTR(buf);'

    if category == 'void':
        call_line = f"    {c_fn}({call_args});"
    else:
        call_line = f"    {ret_type} result = {c_fn}({call_args});"

    # The traced function must not write to stdout: harness stdout IS the cup
    # stream, so any internal printf() lands between cup lines and the runner
    # reports a bogus arity mismatch. Redirect fd 1 to /dev/null around the call.
    call_line = (
        "    fflush(stdout);\n"
        "    int _sv = dup(1);\n"
        "    FILE *_dn = fopen(\"/dev/null\", \"w\");\n"
        "    if (_dn) dup2(fileno(_dn), 1);\n"
        + call_line + "\n"
        "    fflush(stdout);\n"
        "    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }"
    )

    return f"""/* gate: {gate_name} — traced copy of {c_fn} ({category}) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
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


def generate_gap_sentinel(gate_name, py_fn):
    return f"""/* gate: {gate_name} — GAP sentinel for {py_fn} */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {{
    (void)input;
    return "{py_fn}";
}}

int main(int argc, char **argv) {{
    if (argc < 3) {{ fprintf(stderr, "usage: %s {py_fn} <input>...\\n", argv[0]); return 2; }}
    for (int i = 2; i < argc; i++) {{
        WCUP_BEGIN("{py_fn}");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\\n", cup);
        WCUP_END("{py_fn}");
    }}
    return 0;
}}
"""


_FILE_NAMES_CACHE = {}


def _cross_unit_names(c_file, src):
    """Names the file declares as `extern` but does not define.

    Memoized per file: many gates share one ported .c (e.g. 60 gates into
    anthropic_adapter.c), and re-reading + re-regexing it per gate was 85ms
    each — 212s for the full set. Caching drops that to one pass per file.

    Externs only, deliberately. See the note in resolve_deps for why the
    all-called-identifiers variant measured net-negative.
    """
    key = os.path.abspath(c_file)
    if key in _FILE_NAMES_CACHE:
        return _FILE_NAMES_CACHE[key]
    names = set()
    for m in re.finditer(r'extern\s+[\w\s\*]+?(\w+)\s*\([^;{]*\)\s*;', src):
        names.add(m.group(1))
    _FILE_NAMES_CACHE[key] = names
    return names


# Files larger than this are not pulled onto a harness link line by the
# all-called-identifiers sweep. They are orchestration units (web_server.c,
# gateway/run.c) that need include paths and defines the harness does not
# provide, and one of them (web_server.c) has a real json_len collision with
# include/port_web_update.h. Sweeping them turns a linkable gate into a
# compile failure, which is a worse verdict than an honest BUILD FAIL.
MAX_SWEEP_FILE_BYTES = 60_000


_DEFINED_NAMES_CACHE = {}

# A definition is "<type words> name(...) {". Compiled once, not per name.
_DEFINED_RE = re.compile(
    r'(?<![\w])[A-Za-z][\w\s\*]*?\b([a-z_][a-z0-9_]*)\s*\([^;{]*\)\s*\{')


def _defined_names(c_file, src):
    """Names this file DEFINES (has a body for), cached per file.

    Computed with one precompiled regex instead of one compiled-per-candidate
    regex, and cached because many gates share a ported .c.
    """
    key = os.path.abspath(c_file)
    got = _DEFINED_NAMES_CACHE.get(key)
    if got is None:
        got = set(_DEFINED_RE.findall(src))
        _DEFINED_NAMES_CACHE[key] = got
    return got


def resolve_deps(c_file, c_fn, seen=None, max_depth=1):
    """Find the .c files whose symbols a harness needs to LINK.

    A ported .c frequently calls a helper defined in a sibling unit (e.g.
    agent/battery.c calls battery_clear_cache from src/battery.c). Without
    those on the link line the harness fails to BUILD, which the runner
    reports as a divergence — a false RED.

    Depth is bounded at 1 by default. A transitive walk explodes: a function
    that externs a helper whose file externs an auth helper whose file externs
    crypto pulls 40+ units onto the link line, each needing its own deps that
    the harness never provides — so the gate fails to BUILD anyway, just later
    and more confusingly. One hop fixes the common sibling case and stops
    there; anything deeper is a real porting gap, not a harness problem.
    """
    if seen is None:
        seen = set()
    rel = os.path.relpath(c_file, REPO)
    if rel in seen:
        return []
    seen.add(rel)
    deps = [rel]
    try:
        src = open(c_file).read()
    except OSError:
        return deps
    # extern declarations only. Scanning EVERY called identifier was tried and
    # measured net-negative: on the 50-gate alphabetical batch it took matches
    # from 10 to 8 and build-fails from 5 to 8, because it pulls sibling units
    # onto the link line that need include paths/defines the harness lacks (and
    # web_server.c has a real json_len collision with port_web_update.h). The
    # all-identifiers sweep is correct in principle but the port is not yet
    # clean enough for it. Revisit once those compile cleanly.
    for m in re.finditer(r'extern\s+[\w\s\*]+?(\w+)\s*\([^;{]*\)\s*;', src):
        name = m.group(1)
    # A definition has a body: "<ret> name(...) {". A bare occurrence is
    # a call site, not a definition — and the extern declaration itself
    # would otherwise make every cross-unit symbol look local. The prefix
    # class excludes '_' so agent_battery_clear_cache cannot be read as a
    # definition of battery_clear_cache (PoP comments match too).
    #
    # The set of names this file DEFINES is computed once and cached: testing
    # each candidate with its own compiled regex was 3.9s of a 4.3s run
    # (2,840 re.search calls, most of them rebuilding the pattern string).
    for name in sorted(_cross_unit_names(c_file, src)):
        if name in _defined_names(c_file, src):
            continue  # defined locally
        owner = find_definition_cached(name)
        if owner:
            # Skip orchestration-size units: see MAX_SWEEP_FILE_BYTES.
            try:
                if os.path.getsize(owner) > MAX_SWEEP_FILE_BYTES:
                    continue
            except OSError:
                continue
            if max_depth > 1:
                deps += resolve_deps(owner, c_fn, seen, max_depth - 1)
            else:
                # One hop: add the file that defines the symbol, but do not
                # follow ITS externs. Its own unresolved deps are a porting
                # gap, not something the harness can paper over.
                orel = os.path.relpath(owner, REPO)
                if orel not in seen:
                    seen.add(orel)
                    deps.append(orel)
    return deps


def find_definition(name):
    """Locate the .c file that defines `name` as a function."""
    for root, dirs, files in os.walk(os.path.join(REPO, "src")):
        dirs[:] = [d for d in dirs if d != "__pycache__"]
        for fn in files:
            if not fn.endswith(".c"):
                continue
            p = os.path.join(root, fn)
            try:
                txt = open(p, errors="ignore").read()
            except OSError:
                continue
            if re.search(r'^[\w\s\*]+%s\s*\([^;{]*\)\s*\{' % re.escape(name),
                         txt, re.M):
                return p
    return None


_DEF_CACHE = {}


def _build_symbol_index():
    """One pass over src/ mapping every defined function name to its file.

    The per-symbol tree walk was O(gates x files) and did not finish in 180s.
    A single pass builds the same map in ~0.14s for 26,700 symbols across
    1,844 files, which makes wider dependency discovery affordable.
    """
    import re as _re
    pat = _re.compile(
        r'^[A-Za-z][\w\s\*]*?\b([a-z_][a-z0-9_]{2,})\s*\([^;{]*\)\s*\{', _re.M)
    idx = {}
    src_root = os.path.join(REPO, "src")
    for root, dirs, files in os.walk(src_root):
        dirs[:] = [d for d in dirs if d != "__pycache__"]
        for fn in files:
            if not fn.endswith(".c"):
                continue
            p = os.path.join(root, fn)
            try:
                txt = open(p, errors="ignore").read()
            except OSError:
                continue
            for m in pat.finditer(txt):
                idx.setdefault(m.group(1), p)
    return idx


_SYMBOL_INDEX = None


def find_definition_cached(name):
    global _SYMBOL_INDEX
    if _SYMBOL_INDEX is None:
        _SYMBOL_INDEX = _build_symbol_index()
    return _SYMBOL_INDEX.get(name)


def fix_all_gates(check=False):
    # pop_index.json keys carry a stray leading space (historical generator
    # artifact). An unstripped .get() silently misses ~97% of entries, so
    # every ported gate falls through to the gap sentinel. Normalize once.
    with open(POP_INDEX_PATH) as f:
        pop_idx = {k.strip(): v for k, v in json.load(f).items()}
    
    updated = 0
    skipped = 0
    not_gatable = 0
    errors = 0
    
    for d in sorted(os.listdir(GATES)):
        gd = os.path.join(GATES, d)
        if not os.path.isdir(gd):
            continue
        
        gj_path = os.path.join(gd, "gate.json")
        if not os.path.isfile(gj_path):
            continue
        
        g = json.load(open(gj_path))
        py_module = g.get("py_module")
        py_fn = g.get("py_fn")
        
        if not py_module or not py_fn:
            # Legacy gate — skip
            skipped += 1
            continue
        
        key = f"{py_module}:{py_fn}"
        c_info = pop_idx.get(key)
        
        if d.startswith("port__") and c_info:
            # Ported gate → call real C function
            c_fn = c_info["c_fn"]
            c_file = c_info["c_file"]
            
            sig = read_c_signature(c_file, c_fn)
            if sig:
                ret_type, params, param_list = sig
            else:
                ret_type = c_info.get("ret_type", "int")
                params = c_info.get("params", "const char *input")
                param_list = c_info.get("param_list", ["const char *input"])
            
            category = categorize_signature(ret_type, param_list)
            if not category:
                not_gatable += 1
                # A gate previously given a real harness whose signature is
                # NOW known to be unharnessable (e.g. an opaque struct param
                # the categorizer learned to reject) must be reverted to the
                # gap sentinel. Otherwise the stale harness.c stays on disk
                # and keeps failing to compile forever.
                h_path_chk = os.path.join(gd, "harness.c")
                if os.path.isfile(h_path_chk):
                    cur = open(h_path_chk).read()
                    if "gap_sentinel" not in cur:
                        if not check:
                            with open(h_path_chk, 'w') as f:
                                f.write(generate_gap_sentinel(d, py_fn))
                            g["extra_src"] = ["lib/libjson/json.c"]
                            with open(gj_path, 'w') as f:
                                json.dump(g, f, indent=2)
                        updated += 1
                continue
            
            try:
                harness = generate_port_harness(category, d, c_fn, params,
                                                param_list, ret_type,
                                                inputs_walk=g.get("inputs"))
                h_path = os.path.join(gd, "harness.c")
                old = open(h_path).read() if os.path.isfile(h_path) else ""

                deps = [dd for dd in resolve_deps(c_file, c_fn)
                        if dd not in COMMON_SRC]
                want_extra = COMMON_SRC + deps
                harness_changed = old.strip() != harness.strip()
                # gate.json drives the link line, so it must be refreshed too
                # whenever the harness or the dependency set differs — otherwise
                # a corrected resolve_deps never reaches already-written gates.
                gate_changed = (g.get("extra_src") != want_extra
                                or g.get("c_fn") != c_fn)

                if harness_changed or gate_changed:
                    if not check:
                        if harness_changed:
                            with open(h_path, 'w') as f:
                                f.write(harness)
                        g["extra_src"] = want_extra
                        g["c_fn"] = c_fn
                        g["classification"] = "PORTED"
                        with open(gj_path, 'w') as f:
                            json.dump(g, f, indent=2)
                    updated += 1
                else:
                    skipped += 1
            except Exception as e:
                errors += 1
                print(f"  ERROR {d}: {e}")
        
        elif d.startswith("real__"):
            # REAL_GAP → gap sentinel (keep as tracking beacon)
            h_path = os.path.join(gd, "harness.c")
            old = open(h_path).read() if os.path.isfile(h_path) else ""
            
            harness = generate_gap_sentinel(d, py_fn)
            if old.strip() != harness.strip():
                if not check:
                    with open(h_path, 'w') as f:
                        f.write(harness)
                updated += 1
            else:
                skipped += 1
        
        else:
            skipped += 1
    
    action = "Would update" if check else "Updated"
    print(f"{action} {updated} gates, skipped {skipped}, not-gatable {not_gatable}, errors {errors}")


if __name__ == "__main__":
    fix_all_gates(check="--check" in sys.argv)

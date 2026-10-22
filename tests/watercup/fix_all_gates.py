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
# The symbol index walks <REPO>/src. When this module is imported by wc_run
# (whose CWD is the repo root) that is right, but when run directly from
# tests/watercup it would walk the wrong tree and resolve nothing. Derive REPO
# from this file's location instead: <repo>/tests/watercup/fix_all_gates.py.
_HERE = os.path.dirname(os.path.abspath(__file__))
_DERIVED_REPO = os.path.dirname(os.path.dirname(_HERE))
if not os.path.isdir(os.path.join(REPO, "src")) and \
        os.path.isdir(os.path.join(_DERIVED_REPO, "src")):
    REPO = _DERIVED_REPO
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

# symbol -> defining .c file, built once. Lets the per-gate loop resolve a
# gate.json c_fn (repointed to the real prefixed C symbol) to its source file
# without an O(gates x files) walk.
_DEF_RE = re.compile(r'^\s*(?:static\s+)?(?:const\s+)?[A-Za-z_][\w\s\*]*?\b(\w+)\s*\(', re.M)
# Global variable definitions (int g_argc_shared; char *g_token;) also appear
# as undefined references. The function regex misses them; index both shapes.
_GVAR_RE = re.compile(
    r'^\s*(?:static\s+)?(?:const\s+)?[A-Za-z_][\w\s\*]*?\b([a-z_][a-z0-9_]*)\s*'
    r'(?:\[[^\]]*\])?\s*(?:=|;)', re.M)
_SYMBOL_FILE = {}


def _build_symbol_index():
    for root, _, fs in os.walk(os.path.join(REPO, "src")):
        for f in fs:
            if not f.endswith(".c"):
                continue
            path = os.path.join(root, f)
            try:
                txt = open(path, errors="ignore").read()
            except OSError:
                continue
            for sym in _DEF_RE.findall(txt):
                _SYMBOL_FILE.setdefault(sym, path)
            for sym in _GVAR_RE.findall(txt):
                _SYMBOL_FILE.setdefault(sym, path)


_build_symbol_index()


# src/infrastructure_stubs.c is a legacy fallback providing 320 symbols, some of
# which nothing else defines (load_pool, credential_pool_free) — so it must stay
# on the link line. It re-defines exactly ONE symbol that src/cli/paths.c (in
# COMMON_SRC) also provides: hermes_get_home. Linking both is a "multiple
# definition" error. Rename the stub's copy out of the way instead of dropping
# the file: the real paths.c implementation wins.
STUB_SRC = "src/infrastructure_stubs.c"
STUB_SHADOWED = {"hermes_get_home"}

SIG_RE = re.compile(
    r'(?:static\s+)?(?:const\s+)?'
    r'((?:unsigned\s+long\s+long|unsigned\s+long|long\s+long|long|unsigned|[\w]+)'
    r'(?:\s*\*+)?)\s*(\w+)\s*\(([^)]*)\)\s*\{'
)


def read_c_signature(c_file, c_fn):
    if not os.path.isfile(c_file):
        return None
    with open(c_file) as f:
        content = f.read()
    for m in SIG_RE.finditer(content):
        if m.group(2) == c_fn:
            ret_type = m.group(1).strip()
            params = m.group(3).strip()
            param_list = [p.strip() for p in params.split(',') if p.strip()]
            return ret_type, params, param_list
    return None


def categorize_signature(ret_type, param_list, from_real=True):
    unhandleable = ('json', 'struct', 'callback', 'void*', 'fn', 'va_list')
    # Normalize the return type: SIG_RE now captures pointer stars, so a
    # char*-returning fn arrives as "char *" not "char". Strip stars/spaces so
    # the exact-match branches below (ret_type == 'char') still fire.
    ret_type = ret_type.replace('*', '').strip()
    # A pointer-to-pointer param (const char **args, char ***out) is a string
    # ARRAY or nested out-param the harness cannot build from a single argv
    # string. Treating it as char* makes the harness pass one string where the
    # function indexes an array -> SIGSEGV on every input. 139 ported gates
    # take a ** param; reject them (not gatable) rather than crash.
    if any('**' in p for p in param_list):
        return None
    # A bare `void` parameter means NO parameters. Keeping it in the list made
    # every zero-arg function fall through the char/int param tests and get
    # rejected as "not gatable" — e.g. bool _oauth_trace_enabled(void).
    if [p.strip() for p in param_list] == ['void']:
        param_list = []
    # Out-param style: int fn(<inputs...>, char *out, size_t out_size).
    # The RESULT is written into `out`, so the harness must supply a buffer
    # and trace its contents — the int return is usually just a status code.
    # 128 gates use this shape; tracing the return value alone makes every cup
    # constant and mismatched. `out` is second-to-last, its size last.
    # Only trust this when the signature came from the REAL .c (from_real),
    # never from a stale pop_index fallback that may list a single arg.
    if (from_real and ret_type == 'int' and len(param_list) >= 3
            and 'char *' in param_list[-2]
            and 'size_t' in param_list[-1]):
        return 'outparam'
    # A pointer to an opaque/named struct type (coe_engine *eng) is not a
    # string or a number: it cannot be built from argv, and emitting an
    # extern decl with an unknown type makes the harness fail to COMPILE.
    # Only char*/int/double/size_t/json_t*/json_node_t* params are harnessable.
    # Scalar out-pointers (int*/size_t*/double*/bool*) are also harnessable
    # (scalar_outparam category allocates a local and traces what's written).
    _SCALAR_OUT_HEADS = ('int', 'size_t', 'bool', 'long', 'double', 'float',
                         'ssize_t', 'unsigned')
    for p in param_list:
        if '*' in p:
            head = p.split('*')[0].strip()
            if (head and 'char' not in head and 'json_t' not in head
                    and 'json_node_t' not in head
                    and head.replace('const', '').strip() not in _SCALAR_OUT_HEADS):
                return None
        # Match unhandleable tokens against the param TYPE, not the name:
        # "const char *config_json" contains "json" as a NAME substring and was
        # wrongly rejected. The type is all words except the trailing name.
        # json_t/json_node_t are harnessable (parsed via json_parse), so skip
        # the unhandleable "json" token for them.
        words = p.replace('*', ' ').split()
        ptype = ' '.join(words[:-1]) if len(words) > 1 else (words[0] if words else '')
        if 'json_t' in ptype or 'json_node_t' in ptype:
            continue
        for u in unhandleable:
            if u in ptype:
                return None
    # json_t*/json_node_t* return with scalar params: serialize the result via
    # json_serialize and feed the JSON string to the cup. The twin serializes
    # its dict/list return with json.dumps(sort_keys, compact) to match. Params
    # may be char*/int/long/float/double/bool/size_t (numeric params converted
    # like scalar_multi); no struct pointers, no **.
    def _json_scalar_ok(p):
        head = p.split('*')[0].strip()
        if '**' in p:
            return False
        if '*' in p and head and 'char' not in head:
            return False
        return any(t in head for t in
                   ('char', 'int', 'long', 'float', 'double', 'bool',
                    'size_t', 'pid_t', 'ssize_t'))
    if (ret_type.replace('*', '').strip() in ('json_t', 'json_node_t')
            and (not param_list or param_list == ['void']
                 or all(_json_scalar_ok(p) for p in param_list))):
        return 'json_ret'
    # json_t*/json_node_t* PARAM (input): parse the argv string via json_parse,
    # pass the node, serialize the result. Same val encoding as json_ret. Only
    # json/char/scalar params; no other struct pointers, no **.
    def _json_param_ok(p):
        head = p.split('*')[0].strip()
        if '**' in p:
            return False
        if '*' in p:
            if 'char' in head or 'json_t' in head or 'json_node_t' in head:
                return True
            return False  # other struct pointer
        return any(t in head for t in
                   ('char', 'int', 'long', 'float', 'double', 'bool',
                    'size_t', 'pid_t', 'ssize_t'))
    if (param_list and param_list != ['void']
            and all(_json_param_ok(p) for p in param_list)
            and any('json_t' in p or 'json_node_t' in p for p in param_list)
            and not any(u in ret_type for u in ('json_t', 'json_node_t'))):
        return 'json_param'
    # Scalar OUT-params: functions that write results through int*/double*/
    # bool*/size_t*/long* pointers (e.g. turn_summary_count_diff_lines,
    # cc_safe_int, cli_agent_usage_pricing__lookup). Allocate a local for each,
    # pass its address, and trace the written values after the call. Inputs are
    # char*/json/scalar values only (no opaque void* handles, no array inputs,
    # no char* buffer-outs paired with size_t).
    _SCALAR_OUT = ('int', 'size_t', 'bool', 'long', 'double', 'float',
                   'ssize_t', 'unsigned')

    def _is_char_json(p):
        h = p.split('*')[0].strip()
        return 'char' in h or 'json' in h

    def _is_scalar_out(p):
        # const-qualified pointers are INPUTS (the callee must not write
        # through them): treating `const int *token_counts` as an out made
        # the harness pass one zeroed int where the port reads an ARRAY (UB).
        # Array-typed params (`int *pids[]`) are also not single scalar outs.
        if '**' in p or '*' not in p or '[' in p or 'const' in p:
            return False
        return p.split('*')[0].strip() in _SCALAR_OUT

    def _is_scalar_val(p):
        if '*' in p:
            return False
        return any(t in p for t in ('int', 'long', 'float', 'double', 'bool',
                                    'size_t', 'pid_t', 'ssize_t'))
    if (param_list and param_list != ['void']
            and ret_type.replace('*', '').strip() in
            ('int', 'long', 'double', 'float', 'bool', 'char', 'size_t',
             'void', 'json_t', 'json_node_t')
            and any(_is_scalar_out(p) for p in param_list)
            and all(_is_char_json(p) or _is_scalar_out(p) or _is_scalar_val(p)
                    for p in param_list)):
        # char* params are either inputs (passed argv directly) or buffer-outs
        # (a char* param followed by a size_t capacity -> supply a buffer and
        # trace the written string). Both are handled in the harness.
        return 'scalar_outparam'
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
    elif ret_type == 'bool':
        if len(param_list) == 1 and 'char' in param_list[0]:
            return 'bool_str'
        elif len(param_list) == 2 and all('char' in p for p in param_list):
            return 'bool_str_str'
        elif len(param_list) == 1 and 'int' in param_list[0]:
            return 'bool_int'
        elif len(param_list) <= 4 and all('char' in p or 'int' in p for p in param_list):
            return 'bool_other'
    elif ret_type == 'char':
        if len(param_list) <= 2 and all('char' in p or 'int' in p for p in param_list):
            return 'charptr_simple'
    elif ret_type == 'size_t':
        if len(param_list) <= 2 and all('char' in p or 'int' in p for p in param_list):
            return 'sizet_simple'
    elif ret_type == 'long':
        if len(param_list) <= 2 and all('char' in p or 'int' in p or 'long' in p
                                        for p in param_list):
            return 'long_simple'
    elif ret_type == 'double':
        if len(param_list) <= 2 and all('double' in p or 'int' in p for p in param_list):
            return 'double_simple'
    # Generic fallback: any all-scalar-param function (char*/int/long/float/
    # double/bool/size_t, no struct pointers, no **) that the specific
    # categories above didn't match. Covers e.g. char(char,char,char),
    # double(char), int(char,size), etc.
    def _is_scalar_param(p):
        head = p.split('*')[0].strip()
        if '**' in p:
            return False
        if '*' in p and head and 'char' not in head:
            return False  # struct/opaque pointer
        return any(t in head for t in
                   ('char', 'int', 'long', 'float', 'double', 'bool',
                    'size_t', 'pid_t', 'ssize_t'))
    if param_list and all(_is_scalar_param(p) for p in param_list):
        if ret_type in ('int', 'long', 'double', 'float', 'bool', 'char',
                        'size_t', 'void'):
            return 'scalar_multi'
    return None


def generate_port_harness(category, gate_name, c_fn, params, param_list, ret_type,
                          inputs_walk=None, body_src=""):
    # Zero-arg categories pass nothing to the traced function; the input walk
    # still varies the cup via the CUP name + the traced call site.
    noargs = (category.endswith('_noargs') or category == 'void'
              or (category == 'json_ret'
                  and (not param_list or param_list == ['void'])))
    # Categories whose params are ints/doubles must convert the string argv:
    # passing argv[i] straight to an `int` param is a compile error (and
    # silently wrong if it weren't).
    numeric = category in ('int_int', 'bool_int', 'int_other', 'bool_other',
                           'long_simple', 'sizet_simple', 'double_simple',
                           'scalar_multi', 'json_ret')
    # Emit the EXPRESSION for parameter j's argument. `i` is the C loop
    # variable over the INPUT range (argv[2] .. argv[argc-1]); parameters
    # beyond the first take the following walk positions, WRAPPING INSIDE the
    # input range. The wrap must be relative to the input base (argv[2]), not
    # absolute argv: `(i + j) % n` on an absolute i walks off the inputs and
    # reads argv[0]/argv[1] (program name, gate name) as data.
    n_avail = len(inputs_walk) if inputs_walk else 1
    json_param_setup = ''
    json_param_free = ''
    val_code = ''

    def _arg(j):
        if n_avail <= 1 or j == 0:
            return 'argv[i]'
        return 'argv[2 + (i - 2 + %d) %% %d]' % (j, n_avail)

    if noargs:
        call_args = ''
    elif category == 'json_param':
        # Each json_t*/json_node_t* param: parse the argv string into a node
        # via json_parse (local _jN), free after the call. char*/scalar params
        # convert as usual. Setup code is emitted before the call line.
        setup_lines = []
        call_parts = []
        for j, p in enumerate(param_list):
            head = p.split('*')[0].strip()
            if 'json_t' in head or 'json_node_t' in head:
                setup_lines.append(
                    '    json_t *_j%d = json_parse(%s, NULL);' % (j, _arg(j)))
                call_parts.append('_j%d' % j)
            elif 'char' in head:
                call_parts.append(_arg(j))
            elif 'double' in p or 'float' in p:
                call_parts.append('strtod(%s, NULL)' % _arg(j))
            elif 'long' in p or 'size_t' in p or 'ssize_t' in p:
                call_parts.append('(long)strtol(%s, NULL, 10)' % _arg(j))
            else:
                call_parts.append('(int)strtol(%s, NULL, 10)' % _arg(j))
        call_args = ', '.join(call_parts)
        json_param_setup = '\n'.join(setup_lines) + '\n' if setup_lines else ''
        json_param_free = '\n'.join(
            '    if (_j%d) json_free(_j%d);' % (j, j)
            for j, p in enumerate(param_list)
            if 'json_t' in p.split('*')[0] or 'json_node_t' in p.split('*')[0])
    elif category == 'scalar_outparam':
        # Scalar out-params: declare a local per out-pointer, zero it, pass its
        # address, and after the call trace the written values (joined). Inputs
        # (char*/json/scalar) convert as usual. json inputs parse like json_param.
        setup_lines = []
        call_parts = []
        out_decls = []
        out_traces = []
        skip = set()  # indices consumed as a buffer capacity
        for j, p in enumerate(param_list):
            if j in skip:
                continue
            head = p.split('*')[0].strip()
            # char* buffer-out: a NON-const char* param followed by a size_t
            # capacity. A `const char *` is an input, not a writable buffer.
            if ('char' in head and 'const' not in p
                    and j + 1 < len(param_list)
                    and 'size_t' in param_list[j + 1]):
                out_decls.append('    char _s%d[256]; _s%d[0] = 0;' % (j, j))
                call_parts.append('_s%d' % j)
                call_parts.append('sizeof(_s%d)' % j)
                out_traces.append('    WCUP_VALSTR(_s%d);' % j)
                skip.add(j + 1)
                continue
            if 'json_t' in head or 'json_node_t' in head:
                setup_lines.append(
                    '    json_t *_j%d = json_parse(%s, NULL);' % (j, _arg(j)))
                call_parts.append('_j%d' % j)
            elif '*' in p and head.replace('const', '').strip() in (
                    'int', 'size_t', 'bool', 'long', 'double', 'float',
                    'ssize_t', 'unsigned'):
                # scalar out-pointer: local, zeroed, address passed
                ctype = head.replace('const', '').strip()
                out_decls.append('    %s _o%d = 0;' % (ctype, j))
                call_parts.append('&_o%d' % j)
                if ctype in ('double', 'float'):
                    out_traces.append(
                        '    { char _b[64]; snprintf(_b, sizeof(_b), '
                        '"%%.17g", (double)_o%d); WCUP_VALSTR(_b); }' % j)
                elif ctype == 'bool':
                    out_traces.append(
                        '    WCUP_VALSTR(_o%d ? "true" : "false");' % j)
                elif ctype in ('long', 'ssize_t'):
                    out_traces.append(
                        '    { char _b[32]; snprintf(_b, sizeof(_b), "%%ld", '
                        '(long)_o%d); WCUP_VALSTR(_b); }' % j)
                elif ctype == 'size_t':
                    out_traces.append(
                        '    { char _b[32]; snprintf(_b, sizeof(_b), "%%zu", '
                        '(size_t)_o%d); WCUP_VALSTR(_b); }' % j)
                else:  # int / unsigned
                    out_traces.append(
                        '    { char _b[32]; snprintf(_b, sizeof(_b), "%%d", '
                        '(int)_o%d); WCUP_VALSTR(_b); }' % j)
            elif 'char' in head:
                call_parts.append(_arg(j))
            elif 'double' in p or 'float' in p:
                call_parts.append('strtod(%s, NULL)' % _arg(j))
            elif 'long' in p or 'size_t' in p or 'ssize_t' in p:
                call_parts.append('(long)strtol(%s, NULL, 10)' % _arg(j))
            else:
                call_parts.append('(int)strtol(%s, NULL, 10)' % _arg(j))
        call_args = ', '.join(call_parts)
        # Emit out-decls before the call; out-traces replace the normal val_code
        # (they run after the call and emit one WCUP_VALSTR per out-param).
        scalar_outparam_decls = '\n'.join(out_decls) + ('\n' if out_decls else '')
        val_code = '\n'.join(out_traces)
        if 'char' in (ret_type or ''):
            val_code = ('    WCUP_VALSTR(result ? result : "");\n' + val_code) if val_code else '    WCUP_VALSTR(result ? result : "");'
        json_param_setup = (scalar_outparam_decls + json_param_setup)
        json_param_free = '\n'.join(
            '    if (_j%d) json_free(_j%d);' % (j, j)
            for j, p in enumerate(param_list)
            if 'json_t' in p.split('*')[0] or 'json_node_t' in p.split('*')[0])
    elif numeric:
        # Per-param conversion: a `long` param must use strtol WITHOUT the
        # (int) cast (that truncates); double uses strtod; int uses
        # (int)strtol. Casting a long param to int silently loses the value.
        def _conv(j):
            p = param_list[j]
            if 'char' in p.split('*')[0]:
                return _arg(j)  # char* param: pass the argv string directly
            if 'double' in p or 'float' in p:
                return 'strtod(%s, NULL)' % _arg(j)
            if 'long' in p or 'size_t' in p or 'ssize_t' in p:
                return '(long)strtol(%s, NULL, 10)' % _arg(j)
            return '(int)strtol(%s, NULL, 10)' % _arg(j)
        call_args = ', '.join(_conv(j) for j in range(len(param_list)))
    else:
        call_args = ', '.join(_arg(j) for j in range(len(param_list)))

    if category == 'outparam':
        # int fn(<inputs...>, char *out, size_t out_size): supply the buffer,
        # pass its size, and trace what the function WROTE — not the status
        # code it returned.
        n_in = len(param_list) - 2
        in_args = [_arg(j) for j in range(n_in)]
        call_args = ', '.join(in_args + ['_ob', 'sizeof(_ob)'])
        call_line = f"    int result = {c_fn}({call_args});"
        val_code = '    WCUP_VALSTR(_ob);'
    elif category.startswith('int'):
        val_code = '    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);'
    elif category.startswith('bool'):
        val_code = '    WCUP_VALSTR(result ? "true" : "false");'
    elif category == 'charptr_simple':
        val_code = '    WCUP_VALSTR(result ? result : "");'
    elif category == 'json_ret':
        # json_t* return: serialize to a canonical JSON string for the cup.
        # json_serialize emits compact JSON; the twin matches with
        # json.dumps(sort_keys=True, separators=(",", ":")). Free both.
        val_code = ('    char *_js = result ? json_serialize(result) : NULL;\n'
                    '    WCUP_VALSTR(_js ? _js : "null");\n'
                    '    if (result) json_free(result);\n'
                    '    if (_js) free(_js);')
    elif category == 'sizet_simple':
        val_code = '    char buf[32];\n    snprintf(buf, sizeof(buf), "%zu", result);\n    WCUP_VALSTR(buf);'
    elif category == 'long_simple':
        val_code = '    char buf[32];\n    snprintf(buf, sizeof(buf), "%ld", result);\n    WCUP_VALSTR(buf);'
    elif category == 'double_simple':
        val_code = '    char buf[64];\n    snprintf(buf, sizeof(buf), "%.17g", result);\n    WCUP_VALSTR(buf);'
    elif category == 'void':
        val_code = '    WCUP_VALSTR("void");'
    elif category == 'scalar_multi':
        # Generic: encode by the actual return type.
        if ret_type == 'void':
            val_code = '    WCUP_VALSTR("void");'
        elif ret_type == 'bool':
            val_code = '    WCUP_VALSTR(result ? "true" : "false");'
        elif ret_type in ('char',):
            val_code = '    WCUP_VALSTR(result ? result : "");'
        elif ret_type in ('double', 'float'):
            val_code = ('    char buf[64];\n    snprintf(buf, sizeof(buf), '
                        '"%.17g", result);\n    WCUP_VALSTR(buf);')
        elif ret_type in ('long', 'size_t'):
            fmt = '%zu' if ret_type == 'size_t' else '%ld'
            val_code = ('    char buf[32];\n    snprintf(buf, sizeof(buf), '
                        '"' + fmt + '", result);\n    WCUP_VALSTR(buf);')
        else:  # int
            val_code = ('    char buf[32];\n    snprintf(buf, sizeof(buf), '
                        '"%d", result);\n    WCUP_VALSTR(buf);')
    elif category == 'json_param':
        # Return may be json (serialize) or a scalar (encode normally).
        if ret_type.replace('*', '').strip() in ('json_t', 'json_node_t'):
            val_code = ('    char *_js = result ? json_serialize(result) : NULL;\n'
                        '    WCUP_VALSTR(_js ? _js : "null");\n'
                        '    if (result) json_free(result);\n'
                        '    if (_js) free(_js);')
        elif ret_type == 'bool':
            val_code = '    WCUP_VALSTR(result ? "true" : "false");'
        elif ret_type in ('char',):
            val_code = '    WCUP_VALSTR(result ? result : "");'
        elif ret_type in ('double', 'float'):
            val_code = ('    char buf[64];\n    snprintf(buf, sizeof(buf), '
                        '"%.17g", result);\n    WCUP_VALSTR(buf);')
        elif ret_type in ('long', 'size_t'):
            fmt = '%zu' if ret_type == 'size_t' else '%ld'
            val_code = ('    char buf[32];\n    snprintf(buf, sizeof(buf), '
                        '"' + fmt + '", result);\n    WCUP_VALSTR(buf);')
        elif ret_type == 'void':
            val_code = '    WCUP_VALSTR("void");'
        else:  # int
            val_code = ('    char buf[32];\n    snprintf(buf, sizeof(buf), '
                        '"%d", result);\n    WCUP_VALSTR(buf);')
    elif category == 'scalar_outparam':
        # val_code already set to the out-param traces in the call branch.
        if not val_code:
            val_code = '    WCUP_VALSTR("out");'
    else:
        val_code = '    char buf[64];\n    snprintf(buf, sizeof(buf), "%p", (void*)&result);\n    WCUP_VALSTR(buf);'

    if category == 'void' or (category == 'scalar_outparam'
                              and ret_type == 'void'):
        call_line = f"    {c_fn}({call_args});"
    else:
        call_line = f"    {ret_type} result = {c_fn}({call_args});"

    # The traced function must not write to the harness stdout: harness stdout
    # IS the cup stream, so any internal printf() lands between cup lines and
    # the runner reports a bogus arity mismatch. Redirect fd 1 around the call.
    #
    # Many ports print their result and `return 0` (1,154 of them) instead of
    # returning it — Python returns the string, C printf's it. For those the
    # cup must trace the CAPTURED stdout, not the constant 0, or every cup
    # mismatches for a reason that is not a logic difference. Detect the
    # printf-and-return-0 shape and emit a capturing harness for it.
    prints_result = False
    if body_src and category not in ('outparam', 'scalar_outparam'):
        # Scope to THIS function's body: a file-wide "return 1" elsewhere must
        # not disqualify it.
        mb = re.search(r'\b%s\s*\([^;{]*\)\s*\{(.*?)^\}'
                       % re.escape(c_fn), body_src, re.M | re.S)
        fn_body = mb.group(1) if mb else ""
        prints_result = ("printf" in fn_body
                         and re.search(r"return\s+0\s*;", fn_body)
                         and "return 1" not in fn_body)
    if prints_result:
        call_line = (
            json_param_setup
            + "    fflush(stdout);\n"
            + "    int _sv = dup(1);\n"
            + "    FILE *_cap = tmpfile();\n"
            + "    if (_cap) dup2(fileno(_cap), 1);\n"
            + call_line + "\n"
            + "    fflush(stdout);\n"
            + "    if (_cap) {\n"
            + "        dup2(_sv, 1); close(_sv);\n"
            + "        rewind(_cap);\n"
            + "        char _ob[4096]; size_t _on = 0; int _ch;\n"
            + "        while ((_ch = fgetc(_cap)) != EOF && _on < sizeof(_ob) - 1)\n"
            + "            _ob[_on++] = (char)_ch;\n"
            + "        _ob[_on] = '\\0';\n"
            + "        /* strip the trailing newline the port emits */\n"
            + "        while (_on && (_ob[_on-1] == '\\n' || _ob[_on-1] == '\\r'))\n"
            + "            _ob[--_on] = '\\0';\n"
            + "        WCUP_VALSTR(_ob);\n"
            + "        fclose(_cap);\n"
            + "    }\n"
            + json_param_free
        )
    else:
        call_line = (
            json_param_setup
            + "    fflush(stdout);\n"
            + "    int _sv = dup(1);\n"
            + "    FILE *_dn = fopen(\"/dev/null\", \"w\");\n"
            + "    if (_dn) dup2(fileno(_dn), 1);\n"
            + call_line + "\n"
            + "    fflush(stdout);\n"
            + "    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }"
            + json_param_free
        )

    # When the call line captures stdout it already emits WCUP_VALSTR; the
    # normal return-value encoding would add a second, constant value event.
    val_block = "" if prints_result else val_code
    buf_decl = ("    char _ob[4096]; _ob[0] = '\\0';\n"
                if category == 'outparam' else "")

    # Any signature touching json needs the declarations: a scalar_outparam
    # harness with a json_t* return (`json_t *f(const char*, int*)`) declares
    # `json_t *result = f(...)` and failed to compile without json.h.
    json_includes = ('#include "json.h"\n'
                     'extern void json_free(json_t *);\n'
                     'extern char *json_serialize(const json_t *);\n'
                     'extern json_t *json_parse(const char *, char **);\n'
                     if (category in ('json_ret', 'json_param')
                         or 'json' in ret_type
                         or any('json_t' in p or 'json_node_t' in p
                                for p in param_list))
                     else '')
    return f"""/* gate: {gate_name} — traced copy of {c_fn} ({category}) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
{json_includes}

extern {ret_type} {c_fn}({params});

int main(int argc, char **argv) {{
    if (argc < 3) {{ fprintf(stderr, "usage: %s {c_fn} <input>...\\n", argv[0]); return 2; }}
    for (int i = 2; i < argc; i++) {{
        WCUP_BEGIN("{c_fn}");
{buf_decl}{call_line}
{val_block}
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\\n", cup);
        WCUP_END("{c_fn}");
    }}
    return 0;
}}
"""


def _try_link(gate_dir, g, harness_src=None, extra_src=None):
    """Compile+link a gate's harness to a temp binary. Returns True on success.

    Uses the same include roots and vendored-lib link line the runner uses.
    harness_src: if given, use this text as the harness (candidate); else use
    the on-disk harness.c (current). extra_src: override the gate's extra_src.
    """
    import subprocess as _sp
    import tempfile as _tf
    HERE = os.path.join(REPO, "tests/watercup")
    BUILD = os.path.join(REPO, "tests/watercup/build")
    os.makedirs(BUILD, exist_ok=True)
    tag = g["name"]
    hs = os.path.join(gate_dir, "harness.c")
    if harness_src is not None:
        hs = os.path.join(_tf.gettempdir(), "%s.cand.c" % tag)
        with open(hs, "w") as f:
            f.write(harness_src)
    cf = g.get("cflags", [])
    cf = [os.path.join(REPO, c) if not c.startswith("-") else c for c in cf]
    inc = ["-I", HERE, "-I", os.path.join(REPO, "include"),
           "-I", os.path.join(REPO, "lib"), "-I", os.path.join(REPO, "src")]
    try:
        hobj = os.path.join(BUILD, "%s._try.h.o" % tag)
        r = _sp.run(["cc"] + inc + cf + ["-c", hs, "-o", hobj],
                    capture_output=True, text=True, timeout=60)
        if r.returncode != 0:
            return False
        objs = [hobj]
        ex = extra_src if extra_src is not None else g.get("extra_src", [])
        for i, e in enumerate(ex):
            full = os.path.join(REPO, e)
            o = os.path.join(BUILD, "%s._try.e%d.o" % (tag, i))
            r = _sp.run(["cc"] + inc + cf + ["-c", full, "-o", o],
                        capture_output=True, text=True, timeout=60)
            if r.returncode != 0:
                return False
            objs.append(o)
        vend = [p for p in (
            os.path.join(REPO, "lib/libdb/sqlite3.o"),
            os.path.join(REPO, "lib/libhttp/http.o"),
            os.path.join(REPO, "lib/libcrypto/crypto.o"),
            os.path.join(REPO, "lib/libyaml/yaml.o"),
            os.path.join(REPO, "lib/libregex/hermes_regex.o"),
        ) if os.path.exists(p)]
        out = os.path.join(BUILD, "%s._try.bin" % tag)
        r = _sp.run(["cc"] + objs + ["-o", out, "-Wl,--start-group"] + vend
                    + ["-Wl,--end-group", "-lm", "-lcrypto", "-lssl", "-lz",
                       "-ldl", "-lpthread"], capture_output=True, text=True,
                    timeout=60)
        return r.returncode == 0
    except Exception:
        return False


def _current_builds(gate_dir, g):
    return _try_link(gate_dir, g)


def _candidate_builds(gate_dir, g, harness_src, extra_src):
    return _try_link(gate_dir, g, harness_src=harness_src, extra_src=extra_src)


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
    # extern declarations only. An all-called-identifiers sweep was tried
    # twice, at depth 1 and 2, and measured net-negative both times (batch3
    # matches 9 -> 8, batch4 4 -> 3) even after fixing the web_server.c
    # json_len collision that originally blocked it. It pulls sibling units
    # whose own transitive deps stay unresolved at depth 1, and doubling the
    # depth does not recover the matches — it just adds files. Keep the sweep
    # extern-only until the port's cross-unit calls are clean.
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
# Global-variable definitions at FILE SCOPE. Restricted to the codebase's
# g_/s_ naming convention to avoid matching locals inside function bodies
# ("int local_y = 3;") -- a loose pattern would make _defined_names claim a
# file defines locals it doesn't, breaking dep-discovery and over-shadowing
# the stub. Matches "void *g_gw = NULL;" and "gateway_state_t g_state;".
_DEFINED_GVAR_RE = re.compile(
    r'^[\w\s\*]*?\b([gs]_[a-z0-9_]+)\s*(?:\[[^\]]*\])?\s*(?:=|;)', re.M)


def _defined_names(c_file, src):
    """Names this file DEFINES (has a body for), cached per file.

    Computed with one precompiled regex instead of one compiled-per-candidate
    regex, and cached because many gates share a ported .c.

    Includes global-variable definitions (void *g_gw = NULL;
    gateway_state_t g_gw;) alongside functions. The stub-shadow pass needs
    them: infrastructure_stubs.c defines globals that real modules also
    define, and a function-only match misses the collision, so the stub's copy
    is never renamed and the link fails with "multiple definition of g_gw".
    """
    key = os.path.abspath(c_file)
    got = _DEFINED_NAMES_CACHE.get(key)
    if got is None:
        got = set(_DEFINED_RE.findall(src)) | set(_DEFINED_GVAR_RE.findall(src))
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
    """Locate the .c file that defines `name` as a function OR a global var.

    Global definitions (int g_argc_shared; char *g_session_token;) appear as
    undefined references too, but the function-only regex missed them, so the
    retry dropped them and the gate failed to link. Match both shapes.
    """
    fn_re = re.compile(r'^[\w\s\*]+%s\s*\([^;{]*\)\s*\{' % re.escape(name), re.M)
    gv_re = re.compile(
        r'^[\w\s\*]*\b%s\s*(\[[^\]]*\])?\s*(=|;)' % re.escape(name), re.M)
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
            if fn_re.search(txt) or gv_re.search(txt):
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
    # Global variable definitions (int g_argc_shared; char *g_token;) are also
    # undefined references the retry must resolve. Index them alongside funcs.
    gvar = _re.compile(
        r'^[A-Za-z][\w\s\*]*?\b([a-z_][a-z0-9_]{2,})\s*(?:\[[^\]]*\])?\s*(?:=|;)',
        _re.M)
    idx = {}
    stub_path = os.path.join(REPO, "src/infrastructure_stubs.c")
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
            # The stub is a legacy fallback that re-defines symbols real modules
            # own. Record it, but let a REAL module override it: a symbol that
            # resolves to the stub when a real module also defines it makes the
            # link pick the stub, which then gets shadowed away, leaving the
            # symbol undefined. Prefer the real definition.
            is_stub = (os.path.abspath(p) == os.path.abspath(stub_path))
            for m in pat.finditer(txt):
                nm = m.group(1)
                if is_stub:
                    idx.setdefault(nm, p)
                else:
                    idx[nm] = p
            for m in gvar.finditer(txt):
                nm = m.group(1)
                if is_stub:
                    idx.setdefault(nm, p)
                else:
                    idx[nm] = p
    return idx


_SYMBOL_INDEX = None


def find_definition_cached(name):
    global _SYMBOL_INDEX
    if _SYMBOL_INDEX is None:
        _SYMBOL_INDEX = _build_symbol_index()
    return _SYMBOL_INDEX.get(name)


def fix_all_gates(check=False, only_gates=None):
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
        # Targeted mode: only regenerate the named gates. Used to safely
        # regenerate stale-harness gates without mass-regenerating (which
        # regresses working gates via the stub-defer dep-set change).
        if only_gates is not None and d not in only_gates:
            continue
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

        # Ported gate WITHOUT a pop_index entry: the gate.json carries a
        # corrected c_fn (repointed from the PoP annotation to the real
        # prefixed symbol). Look it up in the symbol->file index. Without this
        # these gates keep a stale harness that externs the old phantom symbol
        # and never compile.
        if d.startswith("port__") and not c_info \
                and g.get("classification") == "PORTED":
            fn_guess = g.get("c_fn")
            found = _SYMBOL_FILE.get(fn_guess) if fn_guess else None
            if found:
                c_info = {"c_fn": fn_guess, "c_file": found}

        if d.startswith("port__") and c_info:
            # Ported gate → call real C function
            c_fn = c_info["c_fn"]
            c_file = c_info["c_file"]
            
            sig = read_c_signature(c_file, c_fn)
            if sig:
                ret_type, params, param_list = sig
                from_real = True
            else:
                ret_type = c_info.get("ret_type", "int")
                params = c_info.get("params", "const char *input")
                param_list = c_info.get("param_list", ["const char *input"])
                from_real = False

            category = categorize_signature(ret_type, param_list,
                                            from_real=from_real)
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
                                                inputs_walk=g.get("inputs"),
                                                body_src=open(c_file, errors="ignore").read())
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
                        # SAFETY: only overwrite a gate whose CURRENT harness
                        # builds cleanly if the NEW harness also builds. The
                        # stub-defer / dep-discovery logic can swap a working
                        # infrastructure_stubs.c link for real modules whose
                        # own deps aren't satisfied, regressing a passing gate.
                        # Regenerating a gate whose current harness is broken is
                        # always safe (it couldn't pass before).
                        if _current_builds(gd, g) and not _candidate_builds(
                                gd, g, harness, want_extra):
                            skipped += 1
                            continue
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

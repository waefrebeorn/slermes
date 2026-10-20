#!/usr/bin/env python3
"""gen_watercup_twin_rich.py — rich twin generation for signatures the plain
string-walk template cannot mirror.

Used by gen_watercup_comprehensive.generate_twin when the port's C signature
carries:
  - scalar out-params / char* buffer-outs (the scalar_outparam harness
    category traces the OUT values in param order and ignores the C return
    value, so the twin must trace the Python equivalents of those outs), or
  - json_t*/scalar-by-value inputs (the harness converts fixture strings
    via json_parse / strtol / strtod, so the twin must convert the same way
    or it feeds the live Python function raw strings it cannot accept).

Everything is built with chr(10) joins and character-class regexes so this
file itself contains no backslash escapes.
"""
NL = chr(10)


def _c_out_specs(c_params):
    """Out/buffer specs from a C param list, mirroring the harness
    generator's scalar_outparam detection (const- and array-safe).

    Returns [(kind, ctype, name), ...] in param order; [] when none.
    kind 'buf': a char* buffer-out (with its trailing size_t capacity,
    consumed here). kind 'out': a scalar out-param of the given ctype.
    """
    specs = []
    skip = set()
    pl = [q.strip() for q in (c_params or [])]
    for j, q in enumerate(pl):
        if j in skip:
            continue
        head = q.split('*')[0].strip()
        if ('char' in head and 'const' not in q
                and j + 1 < len(pl) and 'size_t' in pl[j + 1]):
            specs.append(('buf', None, head))
            skip.add(j + 1)
            continue
        if ('*' in q and 'const' not in q and '[' not in q and '**' not in q
                and head in ('int', 'size_t', 'bool', 'long', 'double',
                             'float', 'ssize_t', 'unsigned')):
            name = q.split('*')[-1].strip() or head
            specs.append(('out', head, name))
    return specs


def _twin_needs_conv(c_params):
    """True when the harness converts fixture strings into typed values a
    raw-string-passing twin would not produce: json_t*/json_node_t* params
    (parsed via json_parse) or scalar-by-value params (strtol/strtod)."""
    for q in (c_params or []):
        t = q.strip()
        if 'json_t' in t or 'json_node_t' in t:
            return True
        if '*' in t:
            continue  # pointers: inputs pass argv strings, outs are specs
        tokens = t.replace('const', '').split()
        # drop the trailing parameter NAME when present ('int a' -> 'int')
        if len(tokens) > 1:
            head = ' '.join(tokens[:-1])
        else:
            head = tokens[0] if tokens else ''
        if head in (
                'int', 'long', 'size_t', 'ssize_t', 'double', 'float',
                'bool', 'unsigned', 'pid_t'):
            return True
    return False


def _c_ret_enc_expr(c_ret):
    """Canonical return-value encoding expression for a C return type.

    Shared with the legacy single-value template so both always emit the
    same encoding for the same c_ret.
    """
    if c_ret == 'int':
        return ('str(int(result)) if isinstance(result, (int, bool)) '
                'and not isinstance(result, str) '
                'else ("0" if result is None else str(result))')
    elif c_ret == 'bool':
        return '"true" if result else "false"'
    elif c_ret == 'charptr':
        return ('result if isinstance(result, str) else '
                '("" if result is None else str(result))')
    elif c_ret == 'void':
        return '"void"'
    elif c_ret == 'double':
        return '"%.17g" % float(result)'
    elif c_ret in ('long', 'size_t'):
        return 'str(int(result))'
    elif c_ret == 'json':
        return ('_j.dumps(result, sort_keys=True, separators=(",", ":")) '
                'if result is not None else "null"')
    else:
        return 'str(result)'


def _outvals_src(specs):
    """Source lines for the twin's _outvals/_enc_out helpers: map the live
    Python return value onto the C out-param list, in param order.

    Mapping rules (mirroring how the ports were written):
      - a tuple return whose arity == #outs  -> the whole tuple is the outs
      - a tuple return whose arity == #outs+1 -> component 0 is the C
        return value (status/score); the rest are the outs
      - a single len-named out (count/len/n/size/...) with a container
        return -> len(result)
      - a single buffer-out with any return  -> the return itself
      - anything else raises -> an ERROR cup (honest red, not a fake match)
    """
    specs_lit = repr([(k, t, n) for (k, t, n) in specs])
    lines = [
        "_SPECS = " + specs_lit,
        "_LENY = __import__('re').compile(",
        "    '(count|len|num|size|length|total|^n$)')",
        "",
        "",
        "def _outvals(result):",
        "    n = len(_SPECS)",
        "    if isinstance(result, tuple):",
        "        if len(result) == n:",
        "            return list(result)",
        "        if len(result) == n + 1:",
        "            return list(result[1:])",
        "        raise ValueError('arity:' + str(len(result)))",
        "    if n == 1:",
        "        kind, ct, name = _SPECS[0]",
        "        if kind == 'buf':",
        "            return [result]",
        "        if (isinstance(result, (str, bytes, list, dict, set,",
        "                tuple)) and _LENY.search(name or '')):",
        "            return [len(result)]",
        "        return [result]",
        "    raise ValueError('arity:non-tuple')",
        "",
        "",
        "def _enc_out(sp, v):",
        "    kind, ct, name = sp",
        "    if kind == 'buf':",
        "        if isinstance(v, bytes):",
        "            return v.decode('utf-8', 'replace')",
        "        return v if isinstance(v, str) else str(v)",
        "    if ct == 'bool':",
        "        return 'true' if v else 'false'",
        "    if ct in ('double', 'float'):",
        "        return '%.17g' % float(v)",
        "    return str(int(v))",
        "",
    ]
    return NL.join(lines)


def _conv_src():
    """Source lines for the twin's fixture-conversion helpers: strtol /
    strtod emulation (prefix parse, else 0 — exactly what the C harness's
    strtol/strtod produce for the same fixture string), annotation-driven
    per-param conversion, and the json_parse mirror (invalid JSON -> None,
    the same NULL the C harness passes)."""
    lines = [
        "def _strtol(s):",
        "    m = __import__('re').match('[+-]?[0-9]+', str(s))",
        "    return int(m.group()) if m else 0",
        "",
        "",
        "def _strtod(s):",
        "    m = __import__('re').match(",
        "        '[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?',",
        "        str(s))",
        "    return float(m.group()) if m else 0.0",
        "",
        "",
        "def _cvt(p, v):",
        "    a = str(p.annotation) if p.annotation is not p.empty else ''",
        "    al = a.lower()",
        "    if 'path' in al:",
        "        from pathlib import Path",
        "        return Path(v) if isinstance(v, str) else v",
        "    if 'bytes' in al and 'str' not in al:",
        "        return (v.encode('utf-8', 'surrogateescape')",
        "                if isinstance(v, str) else v)",
        "    if 'bool' in al and 'str' not in al:",
        "        return _strtol(v) != 0",
        "    if ('int' in al and 'point' not in al and 'print' not in al",
        "            and 'interface' not in al):",
        "        return _strtol(v)",
        "    if 'float' in al or 'double' in al:",
        "        return _strtod(v)",
        "    if ('dict' in al or 'list' in al or 'map' in al or 'seq' in al",
        "            or 'tuple' in al or 'set' in al or 'json' in al):",
        "        try:",
        "            import json as _j",
        "            return _j.loads(v)",
        "        except Exception:",
        "            return None",
        "    return v",
        "",
        "",
        "def _call(inp, inputs, idx=0):",
        "    import inspect as _insp",
        "    try:",
        "        _sig = _insp.signature(_FN)",
        "    except Exception:",
        "        _sig = None",
        "    if _sig is not None:",
        "        _pos = [p for p in _sig.parameters.values()",
        "                if p.kind in (p.POSITIONAL_ONLY,",
        "                              p.POSITIONAL_OR_KEYWORD)]",
        "        _n = len(_pos)",
        "        if not _n:",
        "            return _FN()",
        "        _args = []",
        "        n = len(inputs) if inputs else 1",
        "        for j, p in enumerate(_pos):",
        "            raw = inp if j == 0 else inputs[(idx + j) % n]",
        "            _args.append(_cvt(p, raw))",
        "        return _FN(*_args)",
        "    try:",
        "        return _FN(inp)",
        "    except TypeError:",
        "        return _FN()",
        "",
    ]
    return NL.join(lines)


def _resolve_helper_src(py_fn):
    lines = [
        "def _resolve_fn():",
        '    """Module-level fn, else a method on any class in the LIVE module."""',
        '    f = getattr(_pb, "' + py_fn + '", None)',
        "    if f is not None:",
        "        return f",
        "    import inspect as _i",
        "    for _n, _o in vars(_pb).items():",
        "        if _i.isclass(_o) and hasattr(_o, '" + py_fn + "'):",
        "            _m = getattr(_o, '" + py_fn + "')",
        "            if callable(_m):",
        "                return _m.__get__(_DummySelf(), _o)",
        "    return None",
        "",
        "",
        "class _DummySelf:",
        '    """Permissive stand-in for self: any attribute yields another',
        "    _DummySelf, so pure methods run. Methods that need real instance",
        '    state raise and are reported as an ERROR cup — honest, not a false',
        '    MATCH."""',
        "    def __getattr__(self, name):",
        "        return _DummySelf()",
        "    def __call__(self, *a, **k):",
        "        return _DummySelf()",
        "",
        "",
        "_FN = _resolve_fn()",
        "",
    ]
    return NL.join(lines)


def _loader_src(py_module):
    """Twin preamble: load the LIVE Python module inside its real package
    (same loader the legacy template uses)."""
    lines = [
        'HERE = os.path.dirname(os.path.abspath(__file__))',
        'SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(',
        '    os.path.dirname(HERE))))',
        'DEV = os.path.dirname(SLERMES)',
        'sys.path.insert(0, DEV)',
        '',
        'from watercup_py import Cup',
        '',
        '# Relative imports (from .config import ...) need the module loaded',
        '# inside its real package; a bare synthetic name has no package',
        '# context. Register each ancestor package, preferring the REAL one.',
        '_pkg_parts = "' + py_module + '".replace(".py", "").split("/")',
        '_mod_name = ".".join(_pkg_parts)',
        'for _i in range(1, len(_pkg_parts)):',
        '    _anc = ".".join(_pkg_parts[:_i])',
        '    if _anc in sys.modules:',
        '        continue',
        '    try:',
        '        __import__(_anc)',
        '    except Exception:',
        '        import types as _t',
        '        _m = _t.ModuleType(_anc)',
        '        _m.__path__ = [os.path.join(DEV, *_pkg_parts[:_i])]',
        '        _m.__package__ = _anc',
        '        sys.modules[_anc] = _m',
        '_spec = ilu.spec_from_file_location(_mod_name, os.path.join(',
        '    DEV, "' + py_module + '"))',
        '_pb = ilu.module_from_spec(_spec)',
        '_pb.__package__ = ".".join(_pkg_parts[:-1])',
        'sys.modules[_mod_name] = _pb',
        '_spec.loader.exec_module(_pb)',
        '',
    ]
    return NL.join(lines)


def generate_twin_rich(py_module, py_fn, parent_class, c_ret, cup_name,
                       specs):
    """Generate the twin source for out-param / typed-input signatures."""
    loader = _loader_src(py_module)
    resolver = _resolve_helper_src(py_fn)
    conv = _conv_src()
    outs = _outvals_src(specs) if specs else ""

    if parent_class:
        setup = NL.join([
            'try:',
            '    _cls = getattr(_pb, "' + parent_class + '")',
            '    _obj = object.__new__(_cls)',
            'except Exception as e:',
            '    _obj = None',
        ])
        call = NL.join([
            'if _obj is not None:',
            '    result = _obj.' + py_fn + '(inp)',
            'else:',
            '    raise RuntimeError("Cannot instantiate ' + parent_class
            + '")',
        ])
        call_expr = 'result = _obj.' + py_fn + '(inp)'
    else:
        setup = ""
        call_expr = 'result = _call(inp, inputs, idx)'

    if specs:
        trace_expr = NL.join([
            '        for _sp, _v in zip(_SPECS, _outvals(result)):',
            '            c.valstr(_enc_out(_sp, _v))',
        ])
    else:
        enc = _c_ret_enc_expr(c_ret)
        trace_expr = '        c.valstr(' + enc + ')'

    needs_j = (not specs) and (c_ret == 'json')

    head = ('"""twin for ' + py_module + ':' + py_fn
            + ((' (class ' + parent_class + ')') if parent_class else '')
            + '."""')

    lines = [head]
    lines.append('import os, sys, importlib.util as ilu')
    if needs_j:
        lines.append('import json as _j')
    lines.append(loader)
    if setup:
        lines.append(setup)
    lines.append(resolver)
    if outs:
        lines.append(outs)
    lines.append(conv)
    lines.append('def trace(inp, inputs, idx=0):')
    lines.append('    c = Cup("' + (cup_name or py_fn) + '")')
    lines.append('    try:')
    lines.append('        ' + call_expr)
    lines.append(trace_expr)
    lines.append('    except Exception as e:')
    lines.append('        c.valstr(f"ERROR:{type(e).__name__}")')
    lines.append('    return c.finalize()')
    lines.append('')
    lines.append('')
    lines.append('def run(inputs):')
    lines.append('    return [trace(inp, inputs, i) for i, inp'
                 ' in enumerate(inputs)]')
    return NL.join(lines) + NL

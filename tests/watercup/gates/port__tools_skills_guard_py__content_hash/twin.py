"""twin for tools/skills_guard.py:content_hash (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
_SPECS = [('buf', None, 'char')]
_LENY = __import__('re').compile(
    '(count|len|num|size|length|total|^n$)')


def _outvals(result):
    n = len(_SPECS)
    if isinstance(result, tuple):
        if len(result) == n:
            return list(result)
        if len(result) == n + 1:
            return list(result[1:])
        raise ValueError('arity:' + str(len(result)))
    if n == 1:
        kind, ct, name = _SPECS[0]
        if kind == 'buf':
            return [result]
        if (isinstance(result, (str, bytes, list, dict, set,
                tuple)) and _LENY.search(name or '')):
            return [len(result)]
        return [result]
    raise ValueError('arity:non-tuple')


def _enc_out(sp, v):
    kind, ct, name = sp
    if kind == 'buf':
        if isinstance(v, bytes):
            return v.decode('utf-8', 'replace')
        return v if isinstance(v, str) else str(v)
    if ct == 'bool':
        return 'true' if v else 'false'
    if ct in ('double', 'float'):
        return '%.17g' % float(v)
    return str(int(v))

def _strtol(s):
    m = __import__('re').match('[+-]?[0-9]+', str(s))
    return int(m.group()) if m else 0


def _strtod(s):
    m = __import__('re').match(
        '[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?',
        str(s))
    return float(m.group()) if m else 0.0


def _cvt(p, v):
    a = str(p.annotation) if p.annotation is not p.empty else ''
    al = a.lower()
    if 'path' in al:
        from pathlib import Path
        return Path(v) if isinstance(v, str) else v
    if 'bytes' in al and 'str' not in al:
        return (v.encode('utf-8', 'surrogateescape')
                if isinstance(v, str) else v)
    if 'bool' in al and 'str' not in al:
        return _strtol(v) != 0
    if ('int' in al and 'point' not in al and 'print' not in al
            and 'interface' not in al):
        return _strtol(v)
    if 'float' in al or 'double' in al:
        return _strtod(v)
    if ('dict' in al or 'list' in al or 'map' in al or 'seq' in al
            or 'tuple' in al or 'set' in al or 'json' in al):
        try:
            import json as _j
            return _j.loads(v)
        except Exception:
            return None
    return v


class _Twin(TwinBase):
    PY_MODULE = "tools/skills_guard.py"
    PY_FN = "content_hash"
    C_FN = "cli_tools_skills_guard_content_hash"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        for _sp, _v in zip(_SPECS, _outvals(result)):
            c.valstr(_enc_out(_sp, _v))


_t = _Twin()
trace = _t.trace
run = _t.run

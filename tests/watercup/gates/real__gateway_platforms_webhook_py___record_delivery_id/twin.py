"""twin for gateway/platforms/webhook.py:_record_delivery_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
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
    PY_MODULE = "gateway/platforms/webhook.py"
    PY_FN = "_record_delivery_id"
    C_FN = "wh_record_delivery_id"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

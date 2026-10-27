"""twin for agent/context_compressor.py:_content_has_images."""
import os, sys, importlib.util as ilu
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

# Relative imports (from .config import ...) need the module loaded
# inside its real package; a bare synthetic name has no package
# context. Register each ancestor package, preferring the REAL one.
_pkg_parts = "agent/context_compressor.py".replace(".py", "").split("/")
_mod_name = ".".join(_pkg_parts)
for _i in range(1, len(_pkg_parts)):
    _anc = ".".join(_pkg_parts[:_i])
    if _anc in sys.modules:
        continue
    try:
        __import__(_anc)
    except Exception:
        import types as _t
        _m = _t.ModuleType(_anc)
        _m.__path__ = [os.path.join(DEV, *_pkg_parts[:_i])]
        _m.__package__ = _anc
        sys.modules[_anc] = _m
_spec = ilu.spec_from_file_location(_mod_name, os.path.join(
    DEV, "agent/context_compressor.py"))
_pb = ilu.module_from_spec(_spec)
_pb.__package__ = ".".join(_pkg_parts[:-1])
sys.modules[_mod_name] = _pb
_spec.loader.exec_module(_pb)

def _resolve_fn():
    """Module-level fn, else a method on any class in the LIVE module."""
    f = getattr(_pb, "_content_has_images", None)
    if f is not None:
        return f
    import inspect as _i
    for _n, _o in vars(_pb).items():
        if _i.isclass(_o) and hasattr(_o, '_content_has_images'):
            _m = getattr(_o, '_content_has_images')
            if callable(_m):
                return _m.__get__(_DummySelf(), _o)
    return None


class _DummySelf:
    """Permissive stand-in for self: any attribute yields another
    _DummySelf, so pure methods run. Methods that need real instance
    state raise and are reported as an ERROR cup — honest, not a false
    MATCH."""
    def __getattr__(self, name):
        return _DummySelf()
    def __call__(self, *a, **k):
        return _DummySelf()


_FN = _resolve_fn()

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


def _call(inp, inputs, idx=0):
    import inspect as _insp
    try:
        _sig = _insp.signature(_FN)
    except Exception:
        _sig = None
    if _sig is not None:
        _pos = [p for p in _sig.parameters.values()
                if p.kind in (p.POSITIONAL_ONLY,
                              p.POSITIONAL_OR_KEYWORD)]
        _n = len(_pos)
        if not _n:
            return _FN()
        _args = []
        n = len(inputs) if inputs else 1
        for j, p in enumerate(_pos):
            raw = inp if j == 0 else inputs[(idx + j) % n]
            _args.append(_cvt(p, raw))
        return _FN(*_args)
    try:
        return _FN(inp)
    except TypeError:
        return _FN()

def trace(inp, inputs, idx=0):
    c = Cup("cc_content_has_images")
    try:
        result = _call(inp, inputs, idx)
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp, inputs, i) for i, inp in enumerate(inputs)]

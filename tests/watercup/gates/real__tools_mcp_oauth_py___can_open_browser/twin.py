"""twin for tools/mcp_oauth.py:_can_open_browser."""
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
_pkg_parts = "tools/mcp_oauth.py".replace(".py", "").split("/")
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
_spec = ilu.spec_from_file_location(_mod_name, os.path.join(DEV, "tools/mcp_oauth.py"))
_pb = ilu.module_from_spec(_spec)
_pb.__package__ = ".".join(_pkg_parts[:-1])
sys.modules[_mod_name] = _pb
_spec.loader.exec_module(_pb)


def _resolve_fn():
    """Module-level fn, else a method on any class in the LIVE module."""
    f = getattr(_pb, "_can_open_browser", None)
    if f is not None:
        return f
    import inspect as _i
    for _n, _o in vars(_pb).items():
        if _i.isclass(_o) and hasattr(_o, "_can_open_browser"):
            _m = getattr(_o, "_can_open_browser")
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
    via argv[2 + (i - 2 + j) % n]. Using a fixed position for j>0 made a
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
        return _FN(_a0, *[inputs[(idx + j) % n] for j in range(1, _np)])
    try:
        return _FN(inp)
    except TypeError:
        return _FN()

def trace(inp, inputs, idx=0):
    c = Cup("mcpo_u_can_open_browser")
    try:
        result = _call(inp, inputs, idx)
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp, inputs, i) for i, inp in enumerate(inputs)]

"""twin for gateway/shutdown_watchdog.py:cancel."""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_gateway_shutdown_watchdog_py", os.path.join(DEV, "gateway/shutdown_watchdog.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_gateway_shutdown_watchdog_py"] = _pb
_spec.loader.exec_module(_pb)


def _resolve_fn():
    """Module-level fn, else a method on any class in the LIVE module."""
    f = getattr(_pb, "cancel", None)
    if f is not None:
        return f
    import inspect as _i
    for _n, _o in vars(_pb).items():
        if _i.isclass(_o) and hasattr(_o, "cancel"):
            _m = getattr(_o, "cancel")
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
    """
    import inspect as _insp
    try:
        _sig = _insp.signature(_FN)
    except Exception:
        _sig = None
    if _sig is not None:
        _np = sum(
            1 for p in _sig.parameters.values()
            if p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD)
        )
        if _np <= 1:
            return _FN(inp) if _np else _FN()
        n = len(inputs)
        return _FN(inp, *[inputs[(idx + j) % n] for j in range(1, _np)])
    try:
        return _FN(inp)
    except TypeError:
        return _FN()

def trace(inp, inputs, idx=0):
    c = Cup("gateway_shutdown_watchdog_cancel")
    try:
        result = _call(inp, inputs, idx)
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))
    except Exception as e:
        c.valstr(f"ERROR:{type(e).__name__}")
    return c.finalize()


def run(inputs):
    return [trace(inp, inputs, i) for i, inp in enumerate(inputs)]

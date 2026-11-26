"""watercup_twin_runtime — shared live-Python twin engine.

Thin twins declare only their identity (py_module, py_fn, c_fn) and a result
normalization expression; ALL machinery lives here and resolves paths through
watercup_paths rules, so the gate tree works on any OS without regeneration.

The normalization expression is evaluated with `result` bound to the live
return value. It is generator-produced code (trusted), and keeps the exact
per-gate cup semantics the C harness mirrors.
"""
import importlib.util as ilu
import inspect
import os
import sys
import types
from pathlib import Path

from watercup_paths import ensure_sys_path, quarry_root
from watercup_py import Cup

QUARRY = ensure_sys_path()


def load_live(py_module: str):
    """Load a quarry module inside its real package context.

    Relative imports (from .config import ...) need ancestor packages
    registered; prefer the REAL package (__init__.py exports) and fall back
    to a namespace-style module only when the real import fails.
    """
    parts = py_module[:-3].split("/") if py_module.endswith(".py") else py_module.split("/")
    mod_name = ".".join(parts)
    if mod_name in sys.modules:
        return sys.modules[mod_name]
    for i in range(1, len(parts)):
        anc = ".".join(parts[:i])
        if anc in sys.modules:
            continue
        try:
            __import__(anc)
        except Exception:
            m = types.ModuleType(anc)
            m.__path__ = [os.path.join(str(QUARRY), *parts[:i])]
            m.__package__ = anc
            sys.modules[anc] = m
    spec = ilu.spec_from_file_location(mod_name, os.path.join(str(QUARRY), *parts) + ".py")
    if spec is None or spec.loader is None:
        raise ImportError(f"watercup twin: cannot load {py_module} from {QUARRY}")
    mod = ilu.module_from_spec(spec)
    mod.__package__ = ".".join(parts[:-1])
    sys.modules[mod_name] = mod
    spec.loader.exec_module(mod)
    return mod


class _DummySelf:
    """Permissive stand-in for self: any attribute yields another _DummySelf,
    so pure methods run. Methods needing real instance state raise and are
    reported as an ERROR cup — honest, not a false MATCH."""

    def __getattr__(self, name):
        return _DummySelf()

    def __call__(self, *a, **k):
        return _DummySelf()


def resolve_fn(mod, py_fn: str):
    """Module-level fn, else a method on any class in the LIVE module."""
    f = getattr(mod, py_fn, None)
    if f is not None:
        return f
    for _n, obj in vars(mod).items():
        if inspect.isclass(obj) and hasattr(obj, py_fn):
            m = getattr(obj, py_fn)
            if callable(m):
                return m.__get__(_DummySelf(), obj)
    return None


def call_fn(fn, inp, inputs, idx=0):
    """Call the LIVE fn with as many inputs as its signature accepts.

    Param 0 gets the current input; param j>0 gets walk position idx+j
    (wrapping) — the SAME position the C harness feeds via
    argv[2 + (i - 2 + j) % n]. A param annotated Path receives Path(inp).
    """
    try:
        sig = inspect.signature(fn)
    except Exception:
        sig = None
    if sig is not None:
        pos = [p for p in sig.parameters.values()
               if p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD)]
        np = len(pos)
        wrap = pos and "Path" in str(pos[0].annotation)
        a0 = Path(inp) if (wrap and isinstance(inp, str)) else inp
        if np <= 1:
            return fn(a0) if np else fn()
        n = len(inputs)
        return fn(a0, *[inputs[(idx + j) % n] for j in range(1, np)])
    try:
        return fn(inp)
    except TypeError:
        return fn()


class TwinBase:
    """Subclass declares PY_MODULE / PY_FN / C_FN and overrides norm(c, result).

    trace() reproduces the legacy twin contract exactly: a missing live fn
    flows through call_fn(None) -> TypeError -> ERROR:TypeError cup (honest,
    not a false MATCH)."""

    PY_MODULE = ""
    PY_FN = ""
    C_FN = ""

    def __init__(self):
        self._mod = load_live(self.PY_MODULE)
        self._fn = resolve_fn(self._mod, self.PY_FN)

    def norm(self, c, result):
        c.valstr(str(result))

    def trace(self, inp, inputs, idx=0):
        # The cup seed is folded into the hash, so it MUST be the label the harness passes to
        # WCUP_BEGIN. Generated twins carry a C_FN that often differs from it ("numeric" for a
        # harness that begins "account_usage_is_finite_num"), and then every case mismatched no
        # matter what the port returned. wc_run sets _cup_label from the harness source.
        c = Cup(getattr(self, "_cup_label", self.C_FN))
        try:
            result = call_fn(self._fn, inp, inputs, idx)
            self.norm(c, result)
        except Exception as e:
            c.valstr(f"ERROR:{type(e).__name__}")
        return c.finalize()

    def run(self, inputs):
        return [self.trace(inp, inputs, i) for i, inp in enumerate(inputs)]


class Twin(TwinBase):
    """Expression-driven twin (simple normalization only)."""

    def __init__(self, py_module: str, py_fn: str, c_fn: str, valstr_expr: str):
        self.PY_MODULE = py_module
        self.PY_FN = py_fn
        self.C_FN = c_fn
        self.valstr_expr = valstr_expr
        super().__init__()

    def norm(self, c, result):
        c.valstr(eval(self.valstr_expr, {"__builtins__": {}},
                      {"result": result, "str": str, "int": int,
                       "isinstance": isinstance, "bool": bool}))


def make_twin(py_module: str, py_fn: str, c_fn: str, valstr_expr: str) -> Twin:
    return Twin(py_module, py_fn, c_fn, valstr_expr)

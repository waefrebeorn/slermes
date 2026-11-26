"""watercup_twin_runtime — shared live-Python twin engine.

Thin twins declare only their identity (py_module, py_fn, c_fn) and a result
normalization expression; ALL machinery lives here and resolves paths through
watercup_paths rules, so the gate tree works on any OS without regeneration.

The normalization expression is evaluated with `result` bound to the live
return value. It is generator-produced code (trusted), and keeps the exact
per-gate cup semantics the C harness mirrors.
"""
import importlib.util as ilu
import json
import re
import inspect
import os
import sys
import types
from pathlib import Path

from watercup_paths import ensure_sys_path, quarry_root
from watercup_py import Cup

QUARRY = ensure_sys_path()

# Windows CPython has no time.tzset(); a live module that calls it (timezone
# handling in cron/datetime paths) then dies with AttributeError, which reads as a
# twin failure rather than a platform fact. The no-op keeps the twin runnable; the
# platform difference is recorded here, not hidden in a gate.
import time as _time
if not hasattr(_time, "tzset"):
    _time.tzset = lambda: None


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


def _unwrap(obj):
    """A property is not callable but its getter is: gates name properties."""
    if isinstance(obj, property):
        return obj.fget
    return obj


def _dotted(mod, path: str):
    """Walk "Class.attr" / "a.b.c" through getattr. None if any hop is missing."""
    obj = mod
    for part in path.split("."):
        if obj is None:
            return None
        obj = getattr(obj, part, None)
    return obj


def resolve_fn(mod, py_fn: str):
    """Module-level fn, dotted attribute path, else a method on a class in the module.

    A dotted name used to resolve to NOTHING (getattr(mod, "Class.attr") is None and
    hasattr(cls, "Class.attr") is False), so the twin answered ERROR:TypeError for
    every input -- one constant cup, which reads like a port defect. Dotted paths are
    now walked, and a property resolves to its getter.
    """
    if "." in py_fn:
        f = _unwrap(_dotted(mod, py_fn))
        if callable(f):
            return f
        return None
    f = _unwrap(getattr(mod, py_fn, None))
    if callable(f):
        return f
    for _n, obj in vars(mod).items():
        if inspect.isclass(obj) and hasattr(obj, py_fn):
            m = _unwrap(getattr(obj, py_fn))
            if callable(m):
                return m.__get__(_DummySelf(), obj)
    return None


def _strtol(s):
    m = re.match(r"[+-]?[0-9]+", str(s))
    return int(m.group()) if m else 0


def _strtod(s):
    """strtod: a leading number, else 0.0 ("" and "hello" both read as zero, as in C)."""
    if s is None:
        return None
    m = re.match(r"\s*[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?", str(s))
    return float(m.group()) if m else 0.0


def _coerce_param(p, v):
    """Convert the walk's text to the type the live fn annotates.

    call_fn handed every parameter the raw string, so a fn taking a float answered TypeError for
    every input -- an ERROR cup that made the gate look like a port gap. The generated twins carry
    their own _cvt for outputs; this is the same rule for arguments.
    """
    # Harness convention: the text "NULL" is the null pointer, not the four characters. Passing it
    # through as a string made a twin answer "Null" where the C port answered "".
    if isinstance(v, str) and v == "NULL":
        return None
    a = str(p.annotation) if p.annotation is not p.empty else ""
    al = a.lower()
    if not al or "any" in al or al.endswith("str") or al == "str":
        return v
    if "path" in al:
        from pathlib import Path
        return Path(v) if isinstance(v, str) else v
    if "bytes" in al and "str" not in al:
        return v.encode("utf-8", "surrogateescape") if isinstance(v, str) else v
    if al.endswith("bool") or al == "bool":
        if isinstance(v, str) and v.strip().lower() in ("true", "false"):
            return v.strip().lower() == "true"
        return _strtol(v) != 0
    if ("int" in al and "point" not in al and "print" not in al):
        # strtol semantics, which is what the C harness does with the same text: "hello" is 0, not
        # None. Answering None made one side read "$0.00" and the other "".
        return _strtol(v)
    if "float" in al or "double" in al:
        return _strtod(v)
    if any(k in al for k in ("dict", "list", "map", "seq", "tuple", "set", "json")):
        try:
            return json.loads(v)
        except Exception:
            return None
    return v


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
        n = len(inputs)
        vals = [inp] + [inputs[(idx + j) % n] for j in range(1, np)]
        args = [_coerce_param(pos[j], vals[j]) for j in range(np)]
        return fn(*args)
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

    def call(self, inp, inputs, idx=0):
        """Override when the fn needs a real receiver/argument objects.

        The shared walker hands the fn the raw input text, which is right for a
        module-level `f(text)`. A method on a snapshot/config object needs the
        object built from the text first -- otherwise `self` is a permissive dummy
        and every input answers the same cup. Gates whose twin overrides this build
        the receiver with the SAME parse the C harness uses.
        """
        return call_fn(self._fn, inp, inputs, idx)

    def trace(self, inp, inputs, idx=0):
        # The cup seed is folded into the hash, so it MUST be the label the harness passes to
        # WCUP_BEGIN. Generated twins carry a C_FN that often differs from it ("numeric" for a
        # harness that begins "account_usage_is_finite_num"), and then every case mismatched no
        # matter what the port returned. wc_run sets _cup_label from the harness source.
        c = Cup(getattr(self, "_cup_label", self.C_FN))
        # WCUP_BRANCH(n) folds "B<depth>:<0|1>" into the cup. A harness that branches and a twin
        # that does not can never agree, whatever the port answers -- wc_run reads the harness's
        # literal branch and hands it over the same way it hands over the label.
        _b = getattr(self, "_branch", None)
        if _b is not None:
            c.branch(bool(_b))
        try:
            result = self.call(inp, inputs, idx)
            if result is None and not getattr(self, "NORM_NONE", False):
                # C writes `result ? result : ""`, so a None answer is the empty string on that
                # side. Rendering None as the four letters "None" mismatched every gate that can
                # return None. A twin that wants its own rendering sets NORM_NONE = True.
                c.valstr("")
            else:
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

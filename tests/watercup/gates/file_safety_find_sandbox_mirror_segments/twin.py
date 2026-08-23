"""twin for file_safety_find_sandbox_mirror_segments — LIVE
agent/file_safety.py::_find_sandbox_mirror_segments over the path's parts."""
import os
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_fsf", os.path.join(DEV, "agent", "file_safety.py"))
        mod = ilu.module_from_spec(spec)
        sys_modules = __import__("sys").modules
        sys_modules["wc_live_fsf"] = mod
        spec.loader.exec_module(mod)
        return mod._find_sandbox_mirror_segments
    except Exception:
        return None


_live_fn = _load_live()


def _fallback(parts):
    for i, part in enumerate(parts):
        if part != "sandboxes":
            continue
        if i + 5 >= len(parts):
            continue
        if parts[i + 3] == "home" and parts[i + 4] == ".hermes":
            return i + 4
    return None


def trace(raw):
    parts = [p for p in raw.split("/") if p != ""]
    c = Cup("_find_sandbox_mirror_segments")
    for i, part in enumerate(parts):
        is_sb = part == "sandboxes"
        c.branch(is_sb)                                   # per-segment rung
        if not is_sb:
            continue
        shape = (i + 5 < len(parts)) and parts[i + 3] == "home" and parts[i + 4] == ".hermes"
        c.branch(shape)

    result = _live_fn(tuple(parts)) if _live_fn else _fallback(parts)
    c.valstr("-1" if result is None else str(result))
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]

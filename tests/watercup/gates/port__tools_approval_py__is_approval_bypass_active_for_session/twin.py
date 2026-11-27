"""twin: the LIVE session YOLO registry in tools/approval.py.

Same sequence as harness.c: perform the action naming in "<action>|<key>", then read
the membership. The registry is Python's own `_session_yolo`, so a port that prints
and returns false cannot agree with it for any input that enables a session.
"""
from watercup_py import Cup
from watercup_twin_runtime import TwinBase


class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "is_session_yolo_enabled"
    C_FN = "wc_yolo"

    def trace(self, inp, inputs, idx=0):
        import tools.approval as ap
        label = getattr(self, "_cup_label", None) or "wc_yolo"
        c = Cup(label)
        text = "" if inp is None else str(inp)
        action, _, key = text.partition("|")
        c.branch(bool(key))                       # B1
        rc = 0
        try:
            if action == "enable":
                ap.enable_session_yolo(key)
            elif action == "clear":
                ap.clear_session(key)
                rc = 0
        except Exception:
            rc = -1
        active = bool(key) and ap.is_session_yolo_enabled(key)
        c.branch(active)                          # B2
        c.valstr("1" if active else "0")
        return c.finalize()

    def norm(self, c, result):
        c.valstr("1" if result else "0")


_t = _Twin()
trace = _t.trace
run = _t.run

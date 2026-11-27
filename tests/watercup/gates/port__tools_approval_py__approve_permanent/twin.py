"""twin: LIVE tools/approval.py permanent allowlist (Python's own _permanent_set)."""
from watercup_py import Cup
from watercup_twin_runtime import TwinBase


class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "approve_permanent"
    C_FN = "wc_permanent"

    def trace(self, inp, inputs, idx=0):
        import tools.approval as ap
        label = getattr(self, "_cup_label", None) or "wc_permanent"
        c = Cup(label)
        text = "" if inp is None else str(inp)
        action, _, key = text.partition("|")
        c.branch(bool(key))                        # B1
        if action == "approve" and key:
            ap.approve_permanent(key)
        found = bool(key) and key in ap._permanent_set()
        c.branch(found)                            # B2
        c.valstr("1" if found else "0")
        return c.finalize()

    def norm(self, c, result):
        c.valstr("1" if result else "0")


_t = _Twin()
trace = _t.trace
run = _t.run

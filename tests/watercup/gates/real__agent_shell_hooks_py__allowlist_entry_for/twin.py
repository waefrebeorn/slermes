"""twin for agent/shell_hooks.py:allowlist_entry_for (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/shell_hooks.py"
    PY_FN = "allowlist_entry_for"
    C_FN = "allowlist_entry_for"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

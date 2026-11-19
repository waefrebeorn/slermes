"""twin for agent/secret_sources/onepassword.py:override_existing (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/onepassword.py"
    PY_FN = "override_existing"
    C_FN = "opw_override_existing"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

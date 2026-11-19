"""twin for hermes_cli/auth.py:mark_provider_active_if_unset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "mark_provider_active_if_unset"
    C_FN = "mark_provider_active_if_unset"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

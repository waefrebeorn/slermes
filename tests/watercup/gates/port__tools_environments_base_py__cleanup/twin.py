"""twin for tools/environments/base.py:cleanup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/base.py"
    PY_FN = "cleanup"
    C_FN = "hermes_secrets_cleanup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

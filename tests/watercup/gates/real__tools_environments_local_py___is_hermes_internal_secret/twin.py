"""twin for tools/environments/local.py:_is_hermes_internal_secret (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/local.py"
    PY_FN = "_is_hermes_internal_secret"
    C_FN = "env_is_internal_secret"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

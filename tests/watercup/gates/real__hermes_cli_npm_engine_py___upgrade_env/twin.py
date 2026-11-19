"""twin for hermes_cli/npm_engine.py:_upgrade_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/npm_engine.py"
    PY_FN = "_upgrade_env"
    C_FN = "_upgrade_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

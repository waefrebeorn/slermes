"""twin for hermes_cli/npm_engine.py:maybe_repair_npm_engine (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/npm_engine.py"
    PY_FN = "maybe_repair_npm_engine"
    C_FN = "maybe_repair_npm_engine"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

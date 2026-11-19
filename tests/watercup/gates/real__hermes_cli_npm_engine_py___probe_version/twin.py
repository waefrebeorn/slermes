"""twin for hermes_cli/npm_engine.py:_probe_version (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/npm_engine.py"
    PY_FN = "_probe_version"
    C_FN = "_probe_version"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

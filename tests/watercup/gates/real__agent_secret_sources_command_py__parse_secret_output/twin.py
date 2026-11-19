"""twin for agent/secret_sources/command.py:parse_secret_output (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/command.py"
    PY_FN = "parse_secret_output"
    C_FN = "parse_secret_output"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

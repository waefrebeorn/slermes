"""twin for tools/threat_patterns.py:_compile (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/threat_patterns.py"
    PY_FN = "_compile"
    C_FN = "cli_tools_threat_patterns__compile"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

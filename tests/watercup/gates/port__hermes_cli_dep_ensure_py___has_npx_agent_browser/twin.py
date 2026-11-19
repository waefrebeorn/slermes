"""twin for hermes_cli/dep_ensure.py:_has_npx_agent_browser (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dep_ensure.py"
    PY_FN = "_has_npx_agent_browser"
    C_FN = "_has_npx_agent_browser"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

"""twin for hermes_cli/agent_import.py:_run_claude_code (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/agent_import.py"
    PY_FN = "_run_claude_code"
    C_FN = "ai_agent_importer_run_claude_code"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

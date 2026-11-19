"""twin for hermes_cli/cli_agent_setup_mixin.py:_single_query_clarify_callback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_agent_setup_mixin.py"
    PY_FN = "_single_query_clarify_callback"
    C_FN = "_single_query_clarify_callback"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

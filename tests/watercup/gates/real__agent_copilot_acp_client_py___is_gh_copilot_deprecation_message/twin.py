"""twin for agent/copilot_acp_client.py:_is_gh_copilot_deprecation_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/copilot_acp_client.py"
    PY_FN = "_is_gh_copilot_deprecation_message"
    C_FN = "cac_is_gh_copilot_deprecation_message"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

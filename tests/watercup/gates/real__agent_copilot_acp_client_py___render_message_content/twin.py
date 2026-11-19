"""twin for agent/copilot_acp_client.py:_render_message_content (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/copilot_acp_client.py"
    PY_FN = "_render_message_content"
    C_FN = "_render_message_content"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

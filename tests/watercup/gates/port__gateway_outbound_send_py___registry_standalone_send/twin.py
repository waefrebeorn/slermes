"""twin for tools/send_message_tool.py:_registry_standalone_send (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/send_message_tool.py"
    PY_FN = "_registry_standalone_send"
    C_FN = "tools_registry_standalone_send"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

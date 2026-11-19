"""twin for agent/auxiliary_client.py:_read_main_api_key_if_same_host (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_read_main_api_key_if_same_host"
    C_FN = "_read_main_api_key_if_same_host"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

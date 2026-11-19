"""twin for gateway/slash_commands.py:_save_gateway_config_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_commands.py"
    PY_FN = "_save_gateway_config_key"
    C_FN = "_save_gateway_config_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

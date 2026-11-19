"""twin for hermes_cli/web_server.py:_write_memory_provider_config_values (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_write_memory_provider_config_values"
    C_FN = "_write_memory_provider_config_values"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

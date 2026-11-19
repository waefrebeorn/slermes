"""twin for hermes_cli/web_server.py:_install_memory_provider_setup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_install_memory_provider_setup"
    C_FN = "_install_memory_provider_setup"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

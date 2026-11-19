"""twin for agent/image_routing.py:_should_probe_ollama_vision (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/image_routing.py"
    PY_FN = "_should_probe_ollama_vision"
    C_FN = "_should_probe_ollama_vision"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

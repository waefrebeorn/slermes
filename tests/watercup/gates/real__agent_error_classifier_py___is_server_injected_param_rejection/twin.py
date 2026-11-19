"""twin for agent/error_classifier.py:_is_server_injected_param_rejection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/error_classifier.py"
    PY_FN = "_is_server_injected_param_rejection"
    C_FN = "_is_server_injected_param_rejection"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

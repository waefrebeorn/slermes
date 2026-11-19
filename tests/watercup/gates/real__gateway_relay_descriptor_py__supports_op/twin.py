"""twin for gateway/relay/descriptor.py:supports_op (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/descriptor.py"
    PY_FN = "supports_op"
    C_FN = "capability_descriptor_supports_op"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

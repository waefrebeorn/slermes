"""twin for agent/image_gen_provider.py:get_setup_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/image_gen_provider.py"
    PY_FN = "get_setup_schema"
    C_FN = "get_setup_schema"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

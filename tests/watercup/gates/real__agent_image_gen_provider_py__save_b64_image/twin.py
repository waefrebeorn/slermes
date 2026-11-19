"""twin for agent/image_gen_provider.py:save_b64_image (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/image_gen_provider.py"
    PY_FN = "save_b64_image"
    C_FN = "save_b64_image"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

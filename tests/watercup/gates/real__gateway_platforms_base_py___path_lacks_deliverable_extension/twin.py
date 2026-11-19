"""twin for gateway/platforms/base.py:_path_lacks_deliverable_extension (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_path_lacks_deliverable_extension"
    C_FN = "base_platform_path_lacks_deliverable_extension"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

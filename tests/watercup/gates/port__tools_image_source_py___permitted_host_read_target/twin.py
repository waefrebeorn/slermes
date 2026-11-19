"""twin for tools/image_source.py:_permitted_host_read_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/image_source.py"
    PY_FN = "_permitted_host_read_target"
    C_FN = "tools_image_source_u_permitted_host_read_target"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

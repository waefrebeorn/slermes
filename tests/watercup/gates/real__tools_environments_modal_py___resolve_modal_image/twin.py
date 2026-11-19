"""twin for tools/environments/modal.py:_resolve_modal_image (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/modal.py"
    PY_FN = "_resolve_modal_image"
    C_FN = "tools_environments_modal_u_resolve_modal_image"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

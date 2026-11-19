"""twin for tools/environments/docker.py:_image_uses_init_entrypoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/docker.py"
    PY_FN = "_image_uses_init_entrypoint"
    C_FN = "envd_u_image_uses_init_entrypoint"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

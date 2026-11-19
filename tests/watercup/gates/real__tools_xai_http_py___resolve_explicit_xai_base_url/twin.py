"""twin for tools/xai_http.py:_resolve_explicit_xai_base_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/xai_http.py"
    PY_FN = "_resolve_explicit_xai_base_url"
    C_FN = "_resolve_explicit_xai_base_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

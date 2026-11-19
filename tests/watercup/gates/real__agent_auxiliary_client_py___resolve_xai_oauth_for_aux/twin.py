"""twin for agent/auxiliary_client.py:_resolve_xai_oauth_for_aux (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_resolve_xai_oauth_for_aux"
    C_FN = "_resolve_xai_oauth_for_aux"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

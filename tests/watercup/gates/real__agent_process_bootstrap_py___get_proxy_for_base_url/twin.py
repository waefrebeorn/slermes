"""twin for agent/process_bootstrap.py:_get_proxy_for_base_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/process_bootstrap.py"
    PY_FN = "_get_proxy_for_base_url"
    C_FN = "_get_proxy_for_base_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

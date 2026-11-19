"""twin for gateway/relay/__init__.py:_provision_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/__init__.py"
    PY_FN = "_provision_url"
    C_FN = "_provision_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

"""twin for hermes_cli/models.py:_deepinfra_catalog_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_deepinfra_catalog_url"
    C_FN = "models_deepinfra_catalog_url"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

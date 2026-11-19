"""twin for agent/secret_sources/bitwarden.py:install_bws (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/bitwarden.py"
    PY_FN = "install_bws"
    C_FN = "lazy_install_bws"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

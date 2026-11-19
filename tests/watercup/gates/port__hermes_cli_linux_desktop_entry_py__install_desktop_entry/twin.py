"""twin for hermes_cli/linux_desktop_entry.py:install_desktop_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/linux_desktop_entry.py"
    PY_FN = "install_desktop_entry"
    C_FN = "lde_install_desktop_entry"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

"""twin for tools/credential_files.py:clear_credential_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/credential_files.py"
    PY_FN = "clear_credential_files"
    C_FN = "cli_tools_credential_files_clear_credential_files"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

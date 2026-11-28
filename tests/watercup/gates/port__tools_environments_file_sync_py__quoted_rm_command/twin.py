"""twin: tools/environments/file_sync.py:quoted_rm_command.

One cup per gate run; the "|"-joined input is split into the directory list so a
single fixture can carry several paths.
"""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/environments/file_sync.py"
    PY_FN = "quoted_rm_command"
    C_FN = "cli_tools_environments_file_sync_quoted_rm_command"
    def call(self, inp, inputs, idx=0):
        return self._fn(inp.split("|"))
    def norm(self, c, result):
        c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run

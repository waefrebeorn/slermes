"""twin: tools/tts_command_provider.py:shell_quote_context, live.

The module alias must not collide with the TwinBase instance name -- importing as
_tts and instantiating as _t; aliasing both to _t made call() raise
AttributeError and every fixture answered one cup (reported as TWIN BLIND).
"""
import tools.tts_command_provider as _tts
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/tts_command_provider.py"
    PY_FN = "shell_quote_context"
    C_FN = "w14_ttscommand_bd97e2_shell_quote_context"
    NORM_NONE = True
    def call(self, inp, inputs, idx=0):
        pos_s, _, tpl = inp.partition("|")
        return _tts.shell_quote_context(tpl, int(pos_s))
    def norm(self, c, result):
        c.valstr("None" if result is None else result)
_t=_Twin(); trace=_t.trace; run=_t.run

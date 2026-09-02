"""Twin for agent_anthropic_adapter_aa_is_claude_model: _is_claude_model."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from watercup_py import Cup
from agent.anthropic_adapter import _is_claude_model

def trace(input):
    c = Cup("aa_is_claude_model")
    rv = _is_claude_model(input)
    c.valstr("true" if rv else "false")
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]

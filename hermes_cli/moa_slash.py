#!/usr/bin/env python3
"""
Mixture-of-Agents Slash Command Handler for Hermes CLI.

Handles the /moa <prompt> slash command that runs a prompt through
the Mixture of Agents pipeline and returns the synthesized result.
"""

from __future__ import annotations

import asyncio
import json
import sys
from typing import Any

# This will be imported lazily inside the handler to avoid circular imports


def handle_moa_slash_command(agent, prompt: str, mode: str = "standard") -> str:
    """
    Execute a Mixture of Agents run and return the result.
    
    Args:
        agent: The AIAgent instance
        prompt: The user prompt to process
        mode: MoA mode (standard, devil_advocate, trepidation, token_maxx, math)
    
    Returns:
        Formatted response string
    """
    try:
        # Import the MoA tool
        from tools.mixture_of_agents_tool import (
            mixture_of_agents_tool,
            mixture_of_agents_math,
            MoAMode,
        )
        
        # Map mode string to enum
        mode_map = {
            "standard": MoAMode.STANDARD,
            "devil_advocate": MoAMode.DEVIL_ADVOCATE,
            "devil": MoAMode.DEVIL_ADVOCATE,
            "trepidation": MoAMode.TREPIDATION,
            "token_maxx": MoAMode.TOKEN_MAXX,
            "tokenmaxx": MoAMode.TOKEN_MAXX,
            "maxx": MoAMode.TOKEN_MAXX,
            "math": MoAMode.MATH,
        }
        
        moa_mode = mode_map.get(mode.lower(), MoAMode.STANDARD)
        
        # Run the MoA tool
        if moa_mode == MoAMode.MATH:
            result = asyncio.run(mixture_of_agents_math(prompt, moa_mode))
        else:
            result = asyncio.run(mixture_of_agents_tool(
                prompt, 
                moa_mode,
                use_online_research=True,
                research_intent="benchmark_update"
            ))
        
        # Parse and format the result
        try:
            parsed = json.loads(result)
            
            if not parsed.get("success", False):
                return f"❌ MoA Error: {parsed.get('error', 'Unknown error')}"
            
            # Format the output
            output_parts = []
            
            # Header
            output_parts.append(f"🎭 **Mixture of Agents** — Mode: `{mode}`")
            output_parts.append(f"📊 Provider health: {parsed.get('provider_health', {})}")
            output_parts.append("")
            
            # Show reference model responses (briefly)
            ref_responses = parsed.get("reference_responses", [])
            if ref_responses:
                output_parts.append("**📋 Reference Model Responses:**")
                for i, resp in enumerate(ref_responses, 1):
                    provider = resp.get("provider", "unknown")
                    model = resp.get("model", "unknown")
                    content = resp.get("content", "")
                    # Truncate for display
                    preview = content[:200] + "..." if len(content) > 200 else content
                    output_parts.append(f"  {i}. **{provider}:{model}** — {preview}")
                output_parts.append("")
            
            # Aggregator response
            agg_response = parsed.get("aggregator_response", "")
            if agg_response:
                output_parts.append("**🎯 Aggregated Result:**")
                output_parts.append(agg_response)
            
            return "\n".join(output_parts)
            
        except json.JSONDecodeError:
            # Fallback if result isn't JSON
            return f"🎭 **MoA Result:**\n{result}"
            
    except ImportError as e:
        return f"❌ MoA tool not available: {e}. Ensure tools.mixture_of_agents_tool is importable."
    except Exception as e:
        return f"❌ MoA execution failed: {e}"


def handle_moa_slash_command_sync(agent, prompt: str, mode: str = "standard") -> str:
    """Synchronous wrapper for the MoA slash command."""
    return handle_moa_slash_command(agent, prompt, mode)
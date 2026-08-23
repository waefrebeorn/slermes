#!/usr/bin/env python3
"""
Gateway slash command handler for /moa — Mixture of Agents.

This module provides the async gateway handler for the /moa slash command,
which runs a user prompt through the Mixture of Agents pipeline and returns
the synthesized result.
"""

from __future__ import annotations

import asyncio
import json
import logging
from typing import Optional

logger = logging.getLogger(__name__)


async def _handle_moa_command(self, event) -> str:
    """
    Handle /moa <prompt> [mode] command.
    
    Runs a prompt through the Mixture of Agents pipeline using the specified
    mode and returns the aggregated result.
    
    Modes:
    - standard: 2-layer synthesis with diverse models (default)
    - devil_advocate: 3 adversarial critic perspectives  
    - trepidation: Cautious uncertainty quantification
    - token_maxx: Maximum quality, exhaustive tokens
    - math: Specialized mathematical reasoning
    
    Uses FREE models only from:
    - NVIDIA NIM (integrate.api.nvidia.com)
    - NVIDIA Cloud/NVCF (api.nvcf.nvidia.com) 
    - OpenRouter free tier
    - Nous Portal free tier
    
    NEVER uses Anthropic/Claude models.
    """
    from gateway.platforms.base import EphemeralReply
    
    text = (event.text or "").strip()
    
    # Parse /moa <prompt> [mode]
    if not text.startswith("/"):
        return "Usage: /moa <prompt> [mode]\nModes: standard, devil_advocate, trepidation, token_maxx, math"
    
    # Strip the leading /moa
    if text.startswith("/"):
        text = text.lstrip("/")
    if text.startswith("moa"):
        text = text[len("moa"):].lstrip()
    
    if not text:
        return "Usage: /moa <prompt> [mode]\nModes: standard, devil_advocate, trepidation, token_maxx, math"
    
    # Parse mode (last word if it's a valid mode)
    tokens = text.split()
    valid_modes = {"standard", "devil_advocate", "devil", "trepidation", "token_maxx", "tokenmaxx", "maxx", "math"}
    
    mode = "standard"
    prompt = text
    
    # Check if last token is a valid mode
    if tokens and tokens[-1].lower() in valid_modes:
        mode = tokens[-1].lower()
        prompt = " ".join(tokens[:-1])
        if not prompt:
            return "Usage: /moa <prompt> [mode]\nPrompt cannot be empty."
    
    # Normalize mode
    mode_map = {
        "devil": "devil_advocate",
        "tokenmaxx": "token_maxx",
        "maxx": "token_maxx",
    }
    mode = mode_map.get(mode, mode)
    
    try:
        # Import MoA tool
        from tools.mixture_of_agents_tool import (
            mixture_of_agents_tool,
            mixture_of_agents_math,
            MoAMode,
        )
        
        # Map mode string to enum
        mode_map_enum = {
            "standard": MoAMode.STANDARD,
            "devil_advocate": MoAMode.DEVIL_ADVOCATE,
            "trepidation": MoAMode.TREPIDATION,
            "token_maxx": MoAMode.TOKEN_MAXX,
        }
        
        moa_mode = mode_map_enum.get(mode, MoAMode.STANDARD)
        
        # Handle math mode specially (uses standard mode with math-focused models)
        use_math_tool = (mode == "math")
        
        # Run the MoA tool in a thread pool to avoid blocking the event loop
        if use_math_tool:
            from tools.mixture_of_agents_tool import mixture_of_agents_math
            result = await asyncio.to_thread(
                lambda: asyncio.run(mixture_of_agents_math(prompt, MoAMode.STANDARD))
            )
        else:
            result = await asyncio.to_thread(
                lambda: asyncio.run(mixture_of_agents_tool(
                    prompt, 
                    moa_mode,
                    use_online_research=True,
                    research_intent="benchmark_update"
                ))
            )
        
        # Parse and format result
        try:
            parsed = json.loads(result)
            
            if not parsed.get("success", False):
                error = parsed.get("error", "Unknown error")
                return f"❌ **MoA Error**: {error}"
            
            # Format output for gateway
            output_parts = []
            
            # Header
            output_parts.append(f"🎭 **Mixture of Agents** — Mode: `{mode}`")
            
            # Provider health
            health = parsed.get("provider_health", {})
            if health:
                healthy = [p for p, h in health.items() if h.get("healthy", False)]
                unhealthy = [p for p, h in health.items() if not h.get("healthy", False)]
                health_str = f"✅ {', '.join(healthy)}" if healthy else ""
                if unhealthy:
                    health_str += f" | ❌ {', '.join(unhealthy)}"
                if health_str:
                    output_parts.append(f"📊 Provider health: {health_str}")
            
            output_parts.append("")
            
            # Show reference model responses (brief)
            ref_responses = parsed.get("reference_responses", [])
            if ref_responses:
                output_parts.append("**📋 Reference Models:**")
                for i, resp in enumerate(ref_responses[:5], 1):  # Limit to 5
                    provider = resp.get("provider", "unknown")
                    model = resp.get("model", "unknown")
                    content = resp.get("content", "")
                    preview = content[:150] + "..." if len(content) > 150 else content
                    output_parts.append(f"  {i}. **{provider}:{model}** — {preview}")
                if len(ref_responses) > 5:
                    output_parts.append(f"  ... and {len(ref_responses) - 5} more")
                output_parts.append("")
            
            # Aggregator response
            agg_response = parsed.get("aggregator_response", "")
            if agg_response:
                output_parts.append("**🎯 Aggregated Result:**")
                output_parts.append(agg_response)
            
            output_text = "\n".join(output_parts)
            
            # Gateway messages have practical length caps; truncate if needed
            if len(output_text) > 4000:
                output_text = output_text[:4000] + "\n\n... (truncated)"
            
            return EphemeralReply(output_text)
            
        except json.JSONDecodeError:
            # Fallback if result isn't JSON
            return EphemeralReply(f"🎭 **MoA Result**:\n{result[:4000]}")
            
    except ImportError as e:
        logger.error("MoA tool import failed: %s", e)
        return f"❌ MoA tool not available: {e}"
    except Exception as e:
        logger.error("MoA execution failed: %s", e, exc_info=True)
        return f"❌ MoA execution failed: {e}"


def register_moa_slash_handler():
    """
    Register the MoA slash command handler with the gateway.
    
    This should be called during gateway initialization to add the
    /moa command to the slash command dispatch.
    """
    # The handler will be picked up by the gateway's command dispatch
    # via the _handle_moa_command method on the GatewaySlashCommandsMixin
    pass
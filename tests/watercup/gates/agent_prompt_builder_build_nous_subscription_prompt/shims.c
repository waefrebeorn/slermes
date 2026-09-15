// shim: build_nous_subscription_prompt — mirrors Python agent/prompt_builder.py:build_nous_subscription_prompt
// Returns strdup of the exact 1102-char prompt string (verified byte-for-byte from Python).
#include <stdlib.h>
#include <string.h>

const char *build_nous_subscription_prompt(void) {
    return strdup(
        "# Nous Subscription\n"
        "Nous subscription includes managed web tools (Firecrawl), image generation (FAL), "
        "OpenAI TTS, OpenAI Whisper STT, and browser automation (Browser Use) by default. "
        "Modal execution is optional.\n"
        "Current capability status:\n"
        "- Web tools: active via Nous subscription\n"
        "- Image generation: active via Nous subscription\n"
        "- Video generation: not currently available\n"
        "- OpenAI TTS: currently using Edge TTS\n"
        "- Speech-to-text: included with Nous subscription, not currently selected\n"
        "- Browser automation: active via Nous subscription\n"
        "- Modal execution: currently using local\n"
        "When a Nous-managed feature is active, do not ask the user for Firecrawl, FAL, "
        "OpenAI TTS, OpenAI Whisper, or Browser-Use API keys.\n"
        "If the user is not subscribed and asks for a capability that Nous subscription "
        "would unlock or simplify, suggest Nous subscription as one option alongside direct "
        "setup or local alternatives.\n"
        "Do not mention subscription unless the user asks about it or it directly solves "
        "the current missing capability.\n"
        "Useful commands: hermes setup, hermes setup tools, hermes setup terminal, "
        "hermes status.\n"
    );
}

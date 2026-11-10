/*
 * setup_wizard_select.h - setup-wizard provider/backend selectors.
 *
 * Module-internal header (setup_wizard.c <-> setup_wizard_select.c).
 */
#ifndef SETUP_WIZARD_SELECT_H
#define SETUP_WIZARD_SELECT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "setup_wizard_ui.h"   /* WIZ_* colours */

typedef struct {
    const char *name;
    const char *env_var;
    const char *base_url;
    const char *description;
} provider_t;
typedef struct {
    const char *name;
    const char *desc;
} backend_t;

static const provider_t PROVIDERS[] = {
    {"OpenRouter",     "OPENROUTER_API_KEY", "https://openrouter.ai/api/v1",           "200+ models, credit-based"},
    {"OpenAI",         "OPENAI_API_KEY",     "https://api.openai.com/v1",              "GPT-4, GPT-4o, o1, o3"},
    {"Anthropic",      "ANTHROPIC_API_KEY",  "https://api.anthropic.com/v1",            "Claude 3/4 Sonnet, Opus"},
    {"DeepSeek",       "DEEPSEEK_API_KEY",   "https://api.deepseek.com/v1",             "V3, R1, Janus"},
    {"Google/Gemini",  "GOOGLE_API_KEY",     "https://generativelanguage.googleapis.com/v1beta", "Gemini 2.0, 2.5"},
    {"HuggingFace",    "HF_TOKEN",           "https://api-inference.huggingface.co/v1", "Open models, serverless"},
    {"Groq",           "GROQ_API_KEY",       "https://api.groq.com/openai/v1",          "Fast inference, open models"},
    {"Nous Research",  "NOUS_API_KEY",       "https://inference-api.nousresearch.com/v1","Hermes models, inference"},
    {"xAI/Grok",       "XAI_API_KEY",        "https://api.x.ai/v1",                       "Grok 3, reasoning, vision"},
    {NULL, NULL, NULL, NULL}
};
static const backend_t BACKENDS[] = {
    {"local",      "Run commands directly on this machine (default)"},
    {"docker",     "Isolated container with configurable resources"},
    {"ssh",        "Run on a remote machine via SSH"},
    {"modal",      "Serverless cloud sandbox (Modal.com)"},
    {NULL, NULL}
};

void backup_config(const char *path);
void enter_api_key(const char *env_var, const char *base_url);
void select_credential_pool(char *strategy, size_t sz);
void select_image_gen(char *provider, size_t sz);
void select_model(char *model_out, size_t model_sz);
void select_platforms(char *out, size_t sz);
int select_provider(char *env_var, size_t env_sz, char *base_url, size_t url_sz);
void select_terminal_backend(char *backend, size_t sz, char *cpu, size_t cpu_sz, char *mem, size_t mem_sz, char *disk, size_t disk_sz);
void select_tts(char *tts_provider, size_t sz);
void select_web_search(char *provider, size_t sz);

#endif /* SETUP_WIZARD_SELECT_H */

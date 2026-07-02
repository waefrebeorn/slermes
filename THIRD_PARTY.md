# Third-Party Plugins — Port Status

All third-party plugins from the Python Hermes Agent have been ported to C.
Each plugin has a C implementation in `src/plugins/` and a corresponding
`port_*.c` file in `src/cli/` for the Python module functions.

## Platform Adapters (17 adapters)

| Plugin | Python Source | C Port | Status |
|--------|--------------|--------|--------|
| Discord | `plugins/platforms/discord/` | `port_plugins_platforms_discord_adapter.c` + `plugin_platforms.c` | ✅ PORTED |
| Google Chat | `plugins/platforms/google_chat/` | `port_plugins_platforms_google_chat_adapter.c` | ✅ PORTED |
| LINE | `plugins/platforms/line/` | `port_plugins_platforms_line_adapter.c` | ✅ PORTED |
| Mattermost | `plugins/platforms/mattermost/` | `port_plugins_platforms_mattermost_adapter.c` | ✅ PORTED |
| Ntfy | `plugins/platforms/ntfy/` | `port_plugins_platforms_ntfy_adapter.c` | ✅ PORTED |
| Photon | `plugins/platforms/photon/` | `port_plugins_platforms_photon_adapter.c` | ✅ PORTED |
| Simplex | `plugins/platforms/simplex/` | `port_plugins_platforms_simplex_adapter.c` | ✅ PORTED |
| Teams | `plugins/platforms/teams/` | `port_plugins_platforms_teams_adapter.c` | ✅ PORTED |
| IRC | `plugins/platforms/irc/` | `port_plugins_platforms_irc_adapter.c` | ✅ PORTED |
| Home Assistant | `plugins/platforms/homeassistant/` | `port_plugins_platforms_homeassistant_adapter.c` | ✅ PORTED |
| DingTalk | `plugins/platforms/dingtalk/` | `gateway/platforms/dingtalk.c` | ✅ PORTED |
| QQBot | `plugins/platforms/qqbot/` | `gateway/platforms/qqbot_adapter.c` | ✅ PORTED |
| Signal | `plugins/platforms/signal/` | `gateway/platforms/signal.c` | ✅ PORTED |
| Slack | `plugins/platforms/slack/` | `gateway/platforms/slack.c` | ✅ PORTED |
| WhatsApp | `plugins/platforms/whatsapp/` | `gateway/platforms/whatsapp.c` | ✅ PORTED |
| WeCom | `plugins/platforms/wecom/` | `gateway/platforms/wecom.c` | ✅ PORTED |
| Feishu | `plugins/platforms/feishu/` | `gateway/platforms/feishu.c` | ✅ PORTED |
| Matrix | `plugins/platforms/matrix/` | `gateway/platforms/matrix.c` | ✅ PORTED |
| Telegram | `plugins/platforms/telegram/` | `gateway/platforms/telegram.c` | ✅ PORTED |
| Yuanbao | `plugins/platforms/yuanbao/` | `gateway/platforms/yuanbao.c` | ✅ PORTED |

## Model Providers (28 providers)

| Provider | C Implementation |
|----------|-----------------|
| OpenAI | `src/agent/provider_openai.c` |
| Anthropic | `src/agent/provider_anthropic.c` + `src/agent/anthropic_adapter.c` |
| Google (Gemini) | `src/agent/provider_google.c` + `src/agent/gemini_*.c` |
| DeepSeek | `src/agent/provider_deepseek.c` |
| OpenRouter | `src/agent/provider_openrouter.c` |
| Azure OpenAI | `src/agent/provider_azure.c` |
| Bedrock | `src/agent/provider_bedrock.c` |
| Moonshot | `src/agent/provider_moonshot.c` |
| Copilot | `src/agent/provider_copilot.c` |
| LM Studio | `src/agent/lmstudio.c` |
| Ollama | `src/agent/provider_ollama.c` |
| Together | `src/agent/provider_together.c` |
| Fireworks | `src/agent/provider_fireworks.c` |
| Groq | `src/agent/provider_groq.c` |
| Perplexity | `src/agent/provider_perplexity.c` |
| xAI | `src/agent/provider_xai.c` |
| + 12 more | `src/agent/provider_*.c` |

## Memory Plugins

| Plugin | C Implementation | Status |
|--------|-----------------|--------|
| Honcho | `src/plugins/plugin_honcho.c` + `port_plugins_memory_honcho_*.c` | ✅ PORTED |
| Holographic | `src/plugins/plugin_holographic.c` + `port_plugins_memory_holographic_*.c` | ✅ PORTED |
| File Memory | `src/plugins/plugin_file_memory.c` | ✅ PORTED |

## Web Tools

| Tool | C Implementation | Status |
|------|-----------------|--------|
| Brave Search | `port_plugins_web_brave_free_provider.c` | ✅ PORTED |
| DuckDuckGo | `port_plugins_web_ddgs_provider.c` | ✅ PORTED |
| Exa | `port_plugins_web_exa_provider.c` | ✅ PORTED |
| Firecrawl | `port_plugins_web_firecrawl_provider.c` | ✅ PORTED |
| Parallel | `port_plugins_web_parallel_provider.c` | ✅ PORTED |
| SearXNG | `port_plugins_web_searxng_provider.c` | ✅ PORTED |
| Tavily | `port_plugins_web_tavily_provider.c` | ✅ PORTED |
| xAI Search | `port_plugins_web_xai_provider.c` | ✅ PORTED |

## Google Meet

| Module | C Implementation | Status |
|--------|-----------------|--------|
| Meet Bot | `src/plugins/plugin_google_meet.c` + `port_plugins_google_meet_*.c` | ✅ PORTED |
| Audio Bridge | `port_plugins_google_meet_audio_bridge.c` | ✅ PORTED |
| Node Protocol | `port_plugins_google_meet_node_*.c` | ✅ PORTED |

## Teams Pipeline

| Module | C Implementation | Status |
|--------|-----------------|--------|
| Pipeline | `port_plugins_teams_pipeline_pipeline.c` | ✅ PORTED |
| CLI | `port_plugins_teams_pipeline_cli.c` | ✅ PORTED |
| Meetings | `port_plugins_teams_pipeline_meetings.c` | ✅ PORTED |
| Models | `port_plugins_teams_pipeline_models.c` | ✅ PORTED |
| Runtime | `port_plugins_teams_pipeline_runtime.c` | ✅ PORTED |
| Store | `port_plugins_teams_pipeline_store.c` | ✅ PORTED |
| Subscriptions | `port_plugins_teams_pipeline_subscriptions.c` | ✅ PORTED |

## Other Plugins

| Plugin | C Implementation | Status |
|--------|-----------------|--------|
| Spotify | `src/plugins/plugin_spotify.c` + `port_plugins_spotify_*.c` | ✅ PORTED |
| Image Gen | `src/plugins/plugin_image_gen.c` | ✅ PORTED |
| Video Gen | `src/plugins/plugin_video_gen.c` | ✅ PORTED |
| Security Guidance | `src/plugins/plugin_security_guidance.c` | ✅ PORTED |
| Dashboard Auth | `src/plugins/plugin_dashboard_auth.c` | ✅ PORTED |
| Context Engine | `src/plugins/plugin_context_engine.c` | ✅ PORTED |
| Disk Cleanup | `src/plugins/plugin_disk_cleanup.c` | ✅ PORTED |
| Kanban | `src/plugins/plugin_kanban.c` | ✅ PORTED |
| Observability | `src/plugins/plugin_observability.c` | ✅ PORTED |
| Skills | `src/plugins/plugin_skills.c` | ✅ PORTED |
| Achievements | `src/plugins/plugin_achievements.c` | ✅ PORTED |

## Optional Skills

All 55 optional-skill modules are ported to C in `src/cli/port_optional_skills_*.c`:

- **Blockchain**: EVM client, Solana client, Hyperliquid client
- **Creative**: Pixel art, meme generation, ComfyUI workflows, Excalidraw
- **Finance**: Stocks client, DCF model, Excel authoring
- **Health**: Fitness/nutrition calculator
- **MCP**: FastMCP templates (API wrapper, database server, file processor)
- **Migration**: OpenClaw migration scripts
- **ML Ops**: TRL fine-tuning templates
- **Productivity**: Telephony, Memento flashcards, Google Workspace, OCR, PowerPoint
- **Research**: Arxiv search, Polymarket, Drug discovery, Darwinian evolver, OSINT investigation
- **Security**: Godmode, OSS forensics

## Bundled C Libraries

| Library | Purpose | Location |
|---------|---------|----------|
| libjson | JSON parsing | `lib/libjson/` |
| libhttp | HTTP client | `lib/libhttp/` |
| libyaml | YAML parsing | `lib/libyaml/` |
| libssl | SSL/TLS (OpenSSL wrapper) | `lib/libssl/` |
| libwebsocket | WebSocket client | `lib/libwebsocket/` |
| libncurses | Terminal UI | bundled with system |
| whisper-cpp | Local speech-to-text | `lib/whisper_cpp/` |
| libprotobuf | Protocol Buffers | `lib/libprotobuf/` |
| libtoml | TOML parsing | `lib/libtoml/` |
| libregex | Regex engine | `lib/libregex/` |
| libsignal | Signal protocol | `lib/libsignal/` |
| libxai | xAI HTTP client | `lib/libxai_http/` |
| libfal | FAL AI client | `lib/libfal_common/` |
| libmcp | MCP OAuth | `lib/libmcp_oauth/` |
| libcredential | Credential management | `lib/libcredential/` |
| librateguard | Rate limiting | `lib/librateguard/` |
| libenvpassthrough | Environment passthrough | `lib/libenvpassthrough/` |
| libtooloutput | Tool output formatting | `lib/libtooloutput/` |
| libthreatpatterns | Security threat patterns | `lib/libthreatpatterns/` |
| libtextwrap | Text wrapping | `lib/libtextwrap/` |
| libglob | Glob matching | `lib/libglob/` |
| libdifflib | Diff engine | `lib/libdifflib/` |
| libansi | ANSI escape codes | `lib/libansi/` |
| libjson5 | JSON5 parsing | `lib/libjson5/` |
| libcurses_widget | ncurses widgets | `lib/libcurses_widget/` |

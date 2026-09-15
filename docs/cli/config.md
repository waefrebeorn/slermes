<!-- PARITY:AUTO -->
| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | 313 PASS / 22,604 (1.4%) · 0/6230 ported match · 11511/11511 gap mismatch |
**Phase (v672):** PORT phase

**Upstream sync:** 2,072 ahead / 2,639 behind (last merge 2026-09-15 (upstream fetched))

_Generated 2026-09-15T06:22:36Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

# Config Commands

12 commands for managing configuration, models, and behavior.

## `/model` (`/m`)

Model management — list available models, show current model, list providers, set active.

```
/model [list|show|providers|set]
```

**Examples:**
```
/model list
/model show
/model set gpt-4
/model providers
```

## `/config` (`/cfg`)

Show current configuration or set a specific key. CLI-only.

```
/config [key] [val]
```

**Examples:**
```
/config
/config display.pet.enabled true
/config display.pet.scale 0.5
```

## `/setup`

Launch the setup wizard for initial provider/model configuration.

```
/setup [--quick|--non-interactive|--reset|--portal|section]
```

**Examples:**
```
/setup
/setup --quick
/setup model
/setup --reset
```

Sections: `model`, `tts`, `terminal`, `gateway`, `tools`, `agent`

## `/uninstall`

Remove Slermes entirely — deletes binary, config directory, and .env. CLI-only.

```
/uninstall
```

## `/backup`

Back up configuration, .env file, and session data.

```
/backup [config|full]
```

- `config` — Backup config.yaml and .env only
- `full` — Full backup including sessions

## `/topic` (`/t`)

Set the system topic or personality for the conversation.

```
/topic <text>
```

**Example:**
```
/topic You are a helpful coding assistant who writes clean C code.
```

## `/reasoning` (`/re`)

Control reasoning effort level and display settings.

```
/reasoning [on|off|show|hide|low|medium|high]
```

## `/fast`

Toggle fast mode — reduces output verbosity for faster responses.

```
/fast
```

## `/voice`

Toggle voice input/output mode on/off.

```
/voice
```

## `/yolo`

Toggle YOLO mode — skip dangerous command approvals.

```
/yolo
```

## `/personality` (`/p`)

Set a predefined personality as the system message.

```
/personality <name>
```

## `/footer`

Toggle gateway metadata footer on replies.

```
/footer [on|off|status]
```

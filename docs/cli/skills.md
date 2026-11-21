<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Skills Commands

5 commands in the `Skills` category of the live registry (`COMMANDS[]` in
`src/hermes_cli/partial_compress.c`). All handlers live in
`src/hermes_cli/cli_cmd_skills.c`.

## `/skills-hub` (`/hub`)

Skills hub catalog: `list`, `search <query>`, `show <slug>`, `sync` (refresh catalog
from network). The handler fetches the catalog and prints matching skill metadata.
(The same file also contains the wider hub router `hermes_cli_skills_hub_skills_command`
with install/inspect/audit/etc. actions; the slash handler exposes the four subcommands
above.)

```
/skills-hub [list|search <query>|show <slug>|sync]
```

**Examples:**
```
/skills-hub list
/skills-hub search python
/skills-hub show coding
```

## `/skills`

Search and manage installed skills. CLI-only. Prints the skills directory
(`~/.slermes/skills`) and supports `search` / `search-hub <query>` and `install <slug>`.

```
/skills [search <query>|search-hub <query>|install <slug>]
```

## `/bundles`

Lists skill bundles from the bundles directory under the Slermes home.

```
/bundles
```

## `/curator`

Background skill maintenance: `status` (default), `run` (triggers a curator review of
the last tool result), `pause`, `resume`, `help`. State is persisted by the handler.

```
/curator [status|run|pause|resume|help]
```

## `/reload-skills`

Re-scans the skills directory (`~/.slermes/skills`) for changes so new or modified
skills are picked up without restarting.

```
/reload-skills
```

# Tools Commands

## `/tools`

List all available tools and their registration status.

```
/tools
```

Shows each tool's name, whether it's enabled, and its current status.

## `/tools-verify`

Verify that all expected tools are properly registered in the tool registry. CLI-only.

```
/tools-verify
```

Reports any missing tools or registration issues.

## `/commands` (`/cmds`)

List all available slash commands. CLI-only.

```
/commands
```

## `/toolsets`

List available toolsets.

```
/toolsets
```

## `/image`

Attach a local image file to include with the next prompt.

```
/image <path>
```

**Example:**
```
/image screenshot.png
```

## `/paste`

Attach an image from the system clipboard. CLI-only.

```
/paste
```

## `/browser`

Connect the browser tool to a Chromium instance via Chrome DevTools Protocol. CLI-only.

```
/browser
```

## `/deps`

Install third-party Python bridge dependencies. CLI-only.

```
/deps
```

## `/skills`

Search and manage installed skills. CLI-only.

```
/skills
```

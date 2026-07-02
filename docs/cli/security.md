# Security Commands

## `/approve`

Approve a pending dangerous command execution. Gateway-only.

```
/approve
```

## `/deny`

Deny a pending dangerous command execution. Gateway-only.

```
/deny
```

## `/secrets`

Manage secrets: list, get, sync, and status.

```
/secrets [list|get|sync|status]
```

## `/auth`

Show provider authentication status.

```
/auth [status|providers]
```

## `/key`

Manage API keys for different providers.

```
/key [list|set <provider>|show <provider>|unset <provider>]
```

**Examples:**
```
/key list
/key set openai
/key show openai
/key unset openai
```

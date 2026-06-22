# Frontend command completion API

Use `command.complete` for command-bar completion. It replaces both legacy
requests:

- `command.completeText`
- `command.completeId`

Do not combine the structured response with either legacy response.

## Request

Send the complete command-bar text as `partial`:

```ts
{
  protocol: "xen.bridge.v1",
  type: "request",
  name: "command.complete",
  request_id: "completion-42",
  payload: {
    partial: "set ba"
  }
}
```

## Response

The response envelope has the existing bridge shape. Its payload is:

```ts
type CompletionCandidate = {
  // Text to append directly to the current `partial` value.
  insertion: string;

  // Full text to show in the completion menu.
  display: string;

  // Command description. May be empty for intermediate command tokens.
  description: string;

  kind: "command" | "argument";
};

type ActiveArgument = {
  type: string;
  name: string;
  default_value: string | null;
};

type CommandCompletionPayload = {
  candidates: CompletionCandidate[];
  active_argument: ActiveArgument | null;
};
```

Example for `partial: "set ba"`:

```json
{
  "candidates": [
    {
      "insertion": "seFrequency",
      "display": "baseFrequency",
      "description": "Set base frequency in Hz.",
      "kind": "command"
    }
  ],
  "active_argument": null
}
```

The exact description text is catalog data and should not be used as an
identifier.

## Applying a candidate

`insertion` is always an append-only suffix. Do not replace the current token
with `display`.

```ts
function applyCompletion(
  partial: string,
  candidate: CompletionCandidate,
): string {
  return partial + candidate.insertion;
}
```

Examples:

| Current input | Candidate display | Candidate insertion | Result |
| --- | --- | --- | --- |
| `set ba` | `baseFrequency` | `seFrequency` | `set baseFrequency` |
| `set ` | `pitch` | `pitch` | `set pitch` |
| empty input | `set` | `set` | `set` |

Matching is case-insensitive, while returned display and insertion text use the
catalog's canonical spelling.

## Argument hints

When the input has reached a command argument, `active_argument` describes that
argument and an `"argument"` candidate is included for display:

```json
{
  "candidates": [
    {
      "insertion": "",
      "display": "[Float: freq=440]",
      "description": "Set base frequency in Hz.",
      "kind": "argument"
    }
  ],
  "active_argument": {
    "type": "Float",
    "name": "freq",
    "default_value": "440"
  }
}
```

Argument candidates are hints, not text completions. Their `insertion` is empty.
Render `display` or construct richer UI from `active_argument`; do not insert
the bracketed display text into the command.

## Recommended frontend behavior

1. Request completion whenever the command text changes.
2. Render every returned candidate rather than assuming there is only one.
3. Use `display` for menu labels and `description` as optional supporting text.
4. Append `insertion` when a `"command"` candidate is selected.
5. Treat `"argument"` candidates as non-inserting hints.
6. Hide the menu when `candidates` is empty.
7. Ignore an async response if its `request_id` is older than the latest
   completion request.

Minimal request helper:

```ts
async function completeCommand(
  partial: string,
  requestId: string,
): Promise<CommandCompletionPayload> {
  const response = await xenBridgeRequest(JSON.stringify({
    protocol: "xen.bridge.v1",
    type: "request",
    name: "command.complete",
    request_id: requestId,
    payload: { partial },
  }));

  return response.payload as CommandCompletionPayload;
}
```

## Legacy-to-structured mapping

| Legacy usage | Structured replacement |
| --- | --- |
| Append `command.completeText.payload.suffix` | Select a candidate and append its `insertion` |
| Append `command.completeId.payload.id_suffix` | Select a `"command"` candidate and append its `insertion` |
| Show one implicit completion | Render `candidates[]` |
| Show bracketed legacy guide text | Render the `"argument"` candidate or `active_argument` |

The frontend can remove calls, response types, and state associated with
`command.completeText` and `command.completeId` after this migration.

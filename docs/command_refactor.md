# Command refactor status

The backend command refactor is complete:

- Complete chains are expanded and bound before execution.
- One successful submission creates at most one engine-history entry.
- Editor session state is owned separately from engine history.
- `again` stores parsed invocations and rebinds them at replay time.
- Clipboard and saved-measure writes are transactional per submission.
- Deferred staging, `CommitIntent`, and the `commit` command were removed.

The only retained legacy command API is the `complete_text` / `complete_id`
projection used by the webview bridge. It can be removed after the frontend
uses structured completion exclusively.

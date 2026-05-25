# FULIGIN Coworking Protocol

This document defines the collaboration rules for AI agents (Claude, Gemini, etc.) working
on the FULIGIN codebase simultaneously.  Following this protocol prevents merge conflicts,
lost work, and stepping on each other's changes.

---

## Branches

| Branch         | Owner      | Purpose                                  |
|----------------|------------|------------------------------------------|
| `antigravity`  | human      | Primary development branch               |
| `agent/claude` | Claude     | Claude's working branch                  |
| `agent/gemini` | Gemini     | Gemini's working branch                  |

Agents always work on their own branch and submit PRs to `antigravity`.  The human merges.

---

## Protocol Steps

### Before Starting Any Task

1. **Pull the latest code** from `antigravity` into your working branch:
   ```
   git pull origin antigravity
   ```

2. **Read `AGENT_STATE.md`** on the current branch to see what files other agents have locked.
   Do not edit files that another agent has declared as locked.

3. **Update `AGENT_STATE.md`** to declare which files you are locking for this task.
   Format: add your name, current task, and locked file list to the appropriate section.

4. **Commit and push `AGENT_STATE.md` immediately** so other agents can see the lock:
   ```
   git add AGENT_STATE.md
   git commit -m "chore: lock [file list] for [task description]"
   git push origin <your-branch>
   ```

5. **Begin work** — only after the lock is pushed.

### After Completing a Task

6. **Commit all changed files** on your working branch:
   ```
   git add <files>
   git commit -m "<conventional commit message>"
   git push origin <your-branch>
   ```

7. **Update `AGENT_STATE.md`** to mark your task as Idle and release locks.
   Commit and push the updated state.

8. **Notify the human** that a PR is ready.  The human merges to `antigravity`.

---

## AGENT_STATE.md Format

```markdown
# Active Agent Workspace Locks

## Antigravity (Gemini) - Current Branch: agent/gemini, Active Task: <task>, Locked: <files or None>

## Claude - Current Branch: agent/claude, Active Task: <task>, Locked: <files or None>
```

---

## Conflict Resolution

- If two agents need the same file, the one who pushed the lock to GitHub first has priority.
- The second agent waits for the first to finish and push, then pulls and proceeds.
- Never edit a locked file without first checking with the human.

---

## Commit Message Convention

Follow Conventional Commits:
- `feat:` — new feature or UI element
- `fix:` — bug fix
- `refactor:` — code restructuring, no behavior change
- `chore:` — tooling, protocol files, build system
- `style:` — formatting, comments, naming (no logic change)
- `docs:` — documentation only

---

*Last updated: 2026-05-25*

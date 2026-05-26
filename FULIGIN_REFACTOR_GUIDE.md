# FULIGIN — Code Refactoring Guide

A project-local playbook for refactoring FULIGIN's C99 + SDL2 codebase.
Synthesizes the general techniques from
[Wikipedia · Code refactoring](https://en.wikipedia.org/wiki/Code_refactoring)
and
[Augment · 12 Essential Code Refactoring Techniques](https://www.augmentcode.com/guides/12-essential-code-refactoring-techniques)
into rules adapted to this project's tech stack, agent-coworking model, and
known footguns. Read this file before starting any refactor task; it
replaces re-fetching the upstream articles.

---

## Part 1 — Definition & Principles

**Refactoring** is *behaviour-preserving* code restructuring. The external
behaviour (output, side effects, performance budget) of the program is
unchanged; only the internal structure improves — readability,
modularity, testability, maintainability, performance ceiling.

Two cardinal rules:

1. **Refactor in small, individually-verifiable steps.** Each step
   compiles, links, and (where possible) is screenshotted / playtested
   before the next step starts. A 30-edit refactor committed as one blob
   is impossible to bisect when it breaks.
2. **Refactor under green tests / clean builds only.** Never start a
   refactor with the build broken — you cannot tell whether your refactor
   introduced a failure or surfaced a pre-existing one. The first action
   of any refactor session is `make` (or the project equivalent) to
   establish a known-good baseline.

---

## Part 2 — The Core Techniques (synthesis of both sources)

### 2.1 — Extract Function / Extract Method
Pull a logically-cohesive code block into a named function with a clear
input/output contract. Use when a function exceeds ~50 lines, contains a
named comment block (`/* --- step 3 --- */`), or repeats nearby. In C,
prefer `static` for the extracted helper unless it crosses translation
units.

### 2.2 — Inline Function
The inverse: dissolve a one-line trivial function back into its single
caller. Use when the helper adds no abstraction value and the indirection
costs more than it saves.

### 2.3 — Rename Variable / Rename Function
Replace cryptic names (`tmp`, `x2`, `do_thing2`) with intention-revealing
ones (`cooldown_seconds`, `apply_singularity_displacer`). Use the editor's
rename-symbol feature where possible, NOT find-and-replace — find/replace
hits comments and unrelated identifiers.

### 2.4 — Extract Variable (Introduce Explaining Variable)
Replace a complex sub-expression with a named local:
```c
/* Before */
if (player.pos.x > camera_pos.x + SCREEN_WIDTH - 80 && fuel_current < fuel_max * 0.1f) {

/* After */
int   near_right_edge = player.pos.x > camera_pos.x + SCREEN_WIDTH - 80;
float fuel_pct        = fuel_current / fuel_max;
int   fuel_critical   = fuel_pct < 0.1f;
if (near_right_edge && fuel_critical) {
```
Each named expression is its own micro-comment.

### 2.5 — Encapsulate Field
Hide a struct field behind getter/setter functions. In C this is
typically done by making the struct opaque (forward declared in the
public header, full definition in the .c file).

### 2.6 — Move Function / Move Field
Relocate a member from one struct/module to another that uses it more or
"owns" it conceptually. Watch for circular include dependencies — moving
a function across modules often requires breaking a cycle.

### 2.7 — Replace Conditional with Polymorphism
Classic OO move. The C equivalent: replace a long `switch (kind)` with a
function-pointer table or vtable struct. Useful for entity types,
state-machine transitions, weapon-fire callbacks.

### 2.8 — Introduce Parameter Object
A function with 5+ parameters often hides a missing struct. Bundle them
(`SpawnParams`, `RenderContext`).

### 2.9 — Remove Dead Code
Delete functions/branches/fields no one reads. Lean on the compiler:
`-Wunused-function`, `-Wunused-variable`, `-Wunused-parameter`. Dead code
hides bugs (someone "fixes" it without realizing it's dead and breaks
something else).

### 2.10 — Replace Magic Number / Magic String with Constant
Every numeric literal in gameplay code is a future maintenance bug.
Promote to a `#define` or `static const` with a name that explains
*intent*, not just *value*:
```c
/* Bad */
if (sqrtf(dx*dx + dy*dy) < 520.0f) trigger_danger_alert();

/* Good */
#define DANGER_ALERT_DISTANCE  520.0f
if (sqrtf(dx*dx + dy*dy) < DANGER_ALERT_DISTANCE) trigger_danger_alert();
```

### 2.11 — Split Module / Extract Module
A file over ~1500 lines is a smell. Identify a self-contained subsystem
(audio synth, HUD render, save-game I/O), extract its functions and
private state into a new `.c`/`.h` pair, expose only a narrow public API.
This is exactly what `src/ui_hud.c` / `src/state.c` are doing for
`src/game.c` — a refactor still in flight (see Part 4).

### 2.12 — Consolidate Duplicate Code
The DRY rule. Two near-identical functions get merged into one with a
parameter that captures the difference. Three near-identical structures
get a tagged union or a single struct with a type field. Be careful: not
all surface-similar code IS duplicate — two functions that happen to
look alike but evolve independently should stay separate.

---

## Part 3 — Smells That Indicate a Refactor Is Overdue

| Smell | Refactor that addresses it |
|---|---|
| File >1500 lines | Split Module (§2.11) |
| Function >80 lines or >4 nesting levels | Extract Function (§2.1) |
| Same parameter list passed through 3+ functions | Introduce Parameter Object (§2.8) |
| Comment headers like `/* --- BLOCK ---*/` inside one function | Extract Function (§2.1) |
| Magic numbers repeated in 3+ places | Replace Magic Number (§2.10) |
| Variable defined `static` in one .c and as global in another | **Duplicate-symbol bug** — pick one canonical owner; see Part 4 §4.3 |
| `switch (entity_kind)` with >5 cases | Replace Conditional with Polymorphism (§2.7) |
| Functions that no caller reaches (`-Wunused-function`) | Remove Dead Code (§2.9) |
| Identical bug fixed in two places independently | Consolidate Duplicate Code (§2.12) |
| Compile warnings ignored for >1 commit | **Stop and address them** — warnings turn into wrong behaviour |

---

## Part 4 — FULIGIN-Specific Rules (the upstream articles don't cover these)

### 4.1 — The "Never `Write` an existing C file" rule

A file-watcher / linter process on this machine misparses
`#define`/`#undef` inside function bodies and **truncates `src/game.c`,
`src/audio.c`, and other large C files** when a `Write` tool call
creates or appends to a file it's watching. Always use `Edit` or
Desktop Commander's `edit_block` (precise old_string → new_string
replacement). Edit is a diff op and cannot truncate. This rule applies
to ALL agents including sub-agents.

If truncation is suspected: `git diff --stat HEAD` → if line count
collapsed, `git checkout HEAD -- src/<file>.c` and redo edits via Edit.

### 4.2 — Coworking Protocol (multiple agents writing concurrently)

The repo has multiple agents (Claude, Gemini, sometimes the human)
working on `agent/claude`, `agent/gemini`, etc. branches with the
following protocol — see `COWORKING_PROTOCOL.md` for the canonical rules.

1. **Before writing any code: lock the files you intend to touch in
   `AGENT_STATE.md`, commit that lock, and push it to GitHub.** The lock
   must be visible on `fuligin_new/agent/<name>` before implementation
   starts.
2. **Check other agents' locks first.** If a file is locked by someone
   else, either pick a different task or ask the human.
3. **Never edit a file another agent has locked.** Even if their lock
   looks stale.
4. **Release the lock when done.** Set Status to Idle, clear Locked
   Files, add a row to the Work Log table.
5. **All concurrent writers must use `Edit`/`edit_block` not `Write`** —
   see §4.1.
6. **The author of `git commit` is shared across agents** (the human's
   identity), so don't try to detect "who did what" by `author`. Use the
   commit message and the Work Log.

### 4.3 — The Refactor-in-Flight Duplicate-Symbol Trap (a real-world example)

This bit us today. The setup:
- `src/game.c` had `static UpgradeType upgrade_options[3]; static int selected_option;` (file-local).
- A concurrent refactor created `src/state.c` with `UpgradeType upgrade_options[3]; int selected_option;` (non-static global) and `include/state.h` declaring them `extern`.
- `src/ui_hud.c` (also new) renders the upgrade screen reading state.c's globals via state.h.
- `src/game.c` input handler at line 2747 modified the file-local `selected_option`.

Result: **arrow keys updated game.c's static, but the renderer drew
from state.c's global** (always 0). Two copies of the same name in
different scopes were each invisible to the other.

The diagnostic move: `findstr /n /i "upgrade_options selected_option"`
across the project. Multiple non-static defs of the same symbol always
mean a refactor is half-finished.

The fix template (when state.h is also broken so you can't include it):
```c
/* Narrow extern: pull in the specific globals from state.c without
 * inheriting the rest of state.h's (broken) declarations. */
extern UpgradeType upgrade_options[3];
extern int         selected_option;
```
Delete the old file-local statics. Verify both translation units now
write/read the same memory.

### 4.4 — Refactor-Order Discipline

The FULIGIN action list specifies a **Suggested Implementation Order**:

> 1 (dcimgui + two panel primitives) → 2 → 3 → 29 → 6 → 5 → 8 → 14 → 13 → everything else.

Item 1 is the **critical-path foundation** (the two panel primitives).
Any UI refactor that touches HUD or menu rendering must use
`ui_panel_terminal()` or `ui_panel_menu()`, never reinvent panels with
raw `SDL_FRect` calls. Adding a parallel rendering primitive is the
fastest way to introduce a duplicate-style UI smell that someone else
will then have to refactor away.

### 4.5 — Compile-Verify Cadence

A FULIGIN refactor session goes:

```
$ make                    # baseline GREEN
... edit one thing ...
$ make                    # still green? OK, next edit
... edit ...
$ make                    # red? revert ONLY the most recent edit
$ git commit -m "..."    # one logical step per commit
```

`gcc -fsyntax-only -I include -I thirdparty/.../include` is the fast
inner-loop check (~5s vs ~30s for full link). Use it after every edit;
do a full `make` before commit.

### 4.6 — Refactor Commits Are Their Own Type

Commit message prefix conventions in this repo:
- `feat:` — new behaviour visible to the player.
- `fix:` — bug repair.
- `refactor:` — internal restructuring with NO behaviour change.
- `chore:` — lock claims, file moves, build-system tweaks, doc-only.
- `docs:` — README / guide / spec updates.

A `refactor:` commit message MUST explicitly state "no behaviour
change" in the body, and ideally cite the technique applied:

```
refactor: extract render_hud() panels into ui_panel_terminal() (technique 2.1, 2.11)

No behaviour change.  Three near-identical SDL_FRect blocks that drew
the score / fuel / hull panels were extracted into a single call to
ui_panel_terminal().  Eliminates duplicated glow-pass math and centralizes
the Qud bracket-corner aesthetic.
```

### 4.7 — When Refactoring Is the WRONG Move

- **You're behind on a deliverable.** Ship the feature with an ugly
  patch, refactor in the *next* commit.
- **The code you want to refactor has no tests / no playtest plan.**
  Refactoring without verification is just rewriting.
- **You haven't read the surrounding code.** The pattern that looks
  redundant might be load-bearing for a corner case you haven't seen.
- **The build is already broken.** Fix the break first; do not bundle
  refactor changes into a debug session.

---

## Part 5 — Refactor Checklist

Before opening the editor:

- [ ] `make` is green (or the equivalent build command exits 0).
- [ ] AGENT_STATE.md lock claimed and pushed.
- [ ] You know which technique from Part 2 you're applying.
- [ ] You know how you'll verify behaviour preservation (playtest, unit
      test, screenshot diff, or `git diff` review of nearby callers).

During the refactor:

- [ ] Edit in small steps.
- [ ] `gcc -fsyntax-only` after each step.
- [ ] No `Write` on existing C files (§4.1).
- [ ] No edits to files locked by other agents (§4.2).

Before commit:

- [ ] `make` clean.
- [ ] No new compile warnings.
- [ ] `git diff` reviewed — did I accidentally change behaviour?
- [ ] Commit message uses `refactor:` prefix and explicitly states "no
      behaviour change" (§4.6).

After commit:

- [ ] AGENT_STATE.md lock released.
- [ ] Push to `agent/<name>`.
- [ ] Merge to `main` from the main repo.
- [ ] Work Log entry added.

---

*Authors: FULIGIN team (Claude + concurrent agents).
Updated: 2026-05-25.
References: Wikipedia · Code refactoring; Augment · 12 Essential
Refactoring Techniques. Both adapted for C99 + SDL2 + agent-coworking.*

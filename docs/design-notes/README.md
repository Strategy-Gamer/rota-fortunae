# Design Notes

Raw research and AI conversations (ChatGPT, Grok, etc.), pasted here so the
knowledge lives on git instead of in scattered chat histories.

These are **source material**, not authoritative. Distilled conclusions belong in
[`../ARCHITECTURE.md`](../ARCHITECTURE.md) or the relevant `Planning/` doc. When a
note here informs a real decision, summarize it into the architecture doc and link
back to the note.

## How to add a note

1. Create a file: `NN-topic.md` (e.g. `01-multiplayer-multithreading-godot.md`).
2. Paste the conversation (Ctrl-A / Ctrl-V from the chat is fine — raw is okay).
3. Add a top line: source (ChatGPT/Grok), rough date, and one sentence on what it covers.
4. Add it to the index below.

## Index

- **01** — Multiplayer & multithreading in Godot (ChatGPT). Lockstep tick model, command scheduling, prediction, GDScript↔C++ boundary. Distilled into [`../DESIGN.md`](../DESIGN.md) §4–6.
- **02** — Data-oriented design in Godot (ChatGPT). SoA stores, IDs-not-pointers, stores/systems/queries/bridge layering, the C++ map-state port. Distilled into `DESIGN.md` §2–3, §8.
- **03** — Map design principles. (Not yet distilled.)
- **04** — Systematizing game variables (ChatGPT). Stat/modifier engine, effects/triggers/scope, ID registry. Distilled into `DESIGN.md` §7–8 — engine deferred, ID registry adopted.

> Broader game design lives in a separate Google Doc (not on git). The multiplayer
> chat's generated code is already in the repo, so only the design reasoning was pasted.

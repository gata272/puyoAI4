# Chain Blueprint

The large-chain evaluator treats a board as a **sequential chain dependency path** rather than a branching graph.

## Core idea

A dependency `B -> A` exists when removing a prepared B group and applying gravity creates a new 4+ A group. The same colour may appear more than once because groups/events are nodes, not colours. Therefore patterns such as `A -> B -> A -> C` are valid.

The evaluator deliberately does **not** reward simultaneous branches merely because more groups disappear in the same chain wave. The primary structural objective is the length of one sequential path.

## Two levels of analysis

- `quickChainBlueprint()` is a cheap one-step scan used only for optional research tooling.
- `analyzeChainBlueprint()` performs a bounded recursive path search and is applied to the final beam so that the expensive analysis does not multiply across every generated child.

The recursive analysis is bounded to keep the browser implementation practical. It keeps a very small alternative set instead of constructing a full dependency DAG.

## Search policy

Intermediate beam layers continue to use the existing long-chain, prepared-group, form, trigger and height features. The final surviving beam is then scored with the full blueprint and post-trigger tail analysis.

This preserves the existing depth/beam controls and the three-visible-pair information constraint while making long sequential structures much more valuable.

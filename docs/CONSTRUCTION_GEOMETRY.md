# Human-style construction geometry

This revision adds a structural layer to the evaluator without replacing the
existing sequential Main Chain / trigger-route objective.

## Chain units

A stable Puyo board cannot keep a connected group of four or five: it would
already disappear. Therefore `chainUnit4` and `chainUnit5` are **latent** unit
features. They detect exact-three groups that can become four or five through
one physically reachable attachment. This represents the common human pattern
of preparing a 4–5-puyo chain wave before the eventual trigger fires.

Exact-two groups are also examined for a reachable same-colour bridge. This is
a weaker representation of a `2+2` preparation and is deliberately not allowed
to dominate a direct three-puyo anchor.

Groups larger than five are penalized because they spend many puyos on one
chain wave instead of increasing the number of waves.

## Surface shape

The evaluator now measures:

- `surfaceRoughness`: sum of adjacent column-height differences.
- `maxStep`: largest adjacent height difference.
- `heightVariance`: variance of the six column heights.

These do not force a perfectly flat field. They mainly suppress sharp mountains,
valleys, and isolated columns that reduce the number of useful landing surfaces.

## Construction space and tail space

`buildSpace` reserves a bounded amount of empty space above each safe column.
It is capped so an empty board cannot beat a prepared chain merely because it has
more empty cells.

`tailSpace` rewards top landing cells whose neighboring surfaces are within one
row. This represents the flat receiving surface that is useful for both chain
tails and the next Main Chain unit.

`deadSpace` detects holes below a column top. Normal simulator states should
have zero holes; the feature mainly protects edited/debug states and future
search extensions.

## Edge walls

`edgeWall` gives a small positive signal when both sides are somewhat higher
than the center while avoiding a uniformly high board. It is intentionally soft:
edge height is a supporting geometry feature, not the objective by itself.

## Interaction with the existing policy

The new terms are added as tunable weights in Developer Mode. The existing
sequential trigger-route score, Main Chain continuity, cleanup policy, actual
chain reward, game-over fallback, and 3-pair visible-information constraint are
kept intact.

The most important intended behavior is:

1. preserve one Main Chain;
2. prepare 3-puyo anchors and latent 4/5-puyo chain units;
3. keep the surface usable and avoid sharp height changes;
4. retain room above and around the chain tail;
5. avoid cashing out merely because a trigger is available;
6. fire when the accumulated dependency path is sufficiently valuable.

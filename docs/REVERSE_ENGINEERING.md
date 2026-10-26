# From binary structure to a frame-safe visualization

The original Arma Reforger controller was built from a sequence of hypotheses, static inspection, bounded live reads, and deterministic fixtures. These notes highlight the parts where that sequence changed the implementation. They describe **Arma Reforger 1.8.0.13** only. This public preview retains the renderer and fake-memory fixtures; its live read transport is a placeholder.

## Evidence discipline

An offset seen in a decompiler or supplied by an outside schema is a candidate. A useful chain must also produce sane values in a live read-only sample and survive the transition cases that occur during play. The controller then has one more obligation: a failed read must not leave a partial or stale frame on screen. Its fake-memory tests exercise those failure boundaries without a game process.

The [architecture ledger](../STRUCTURES.md) records the complete chains, per-frame read costs, and insertion points. The table below is a compact map of the findings that most shaped the final controller.

| Area | Finding | Why it mattered |
| --- | --- | --- |
| World enumeration | The active array is read from `World+0x150`; its count is at `World+0x15C`. | A nearby `+0x158` value looked count-like but disagreed with live traversal. The scanner validates and bounds the actual count before walking it. |
| Character identity | The class-name path resolves through `entity+0xE8`, then `+0x08`, then `+0x10`; admission requires exact `SCR_ChimeraCharacterClass`. | Skeleton and ordinary character visualizations share one precise filter. Fuzzy name matching remains diagnostic-only. |
| Weak references | A weak field leads to a control block, whose first pointer is an interior object pointer; the complete object is recovered with a `0x10` bias. | Treating the weak field as a direct object pointer produced invalid chains. Rechecking both links catches transitions before publication. |
| Camera | The active camera supplies right/up/forward vectors at `+0x80/+0x8C/+0x98`, position at `+0xA4`, and the projection scale at `+0x194`. | World-to-screen projection uses the observed basis and FOV convention. The camera is sampled first in the rendered-frame phase. |
| Skeleton | Matrix data comes through `entity+0x50 → mesh+0x30 → owner+0x10`; entries are `0x30` bytes with translation at `+0x24`. Bone count uses a different path through mesh component data and `MeshObject+0xD0`. | Three live characters reported 231 bones. A mistaken shortcut at `mesh+0xD0` returned garbage, so count and matrix endpoints are validated separately. |
| Game Master camera | The exact editor-manager entity had a 20-component array; movement data was observed at slot 18. The live quaternion was at `+0x68`, position at `+0x80`, and active state at entity `+0x100`. | Initial `+0x70` position and `+0x90` quaternion candidates were falsified. The indicator now requires a fresh active-state sample and clears itself when inactive. |

## Case study: the character rig

The first bone-matrix reads produced plausible local-space translations, but a plausible head point alone was not enough to draw a whole skeleton. Live diagnostics printed the full array for multiple exact-class characters, including motion samples. Midline joints rose from pelvis through spine to head; paired arm and leg chains moved symmetrically and, for the walking character, the limb endpoints changed while upper-body anchors remained recognizable.

The selected line set uses pelvis `10`; spine `31/36/41`; neck/head `81/83`; arms `45/48/51` and `195/198/201`; and legs `18/19/23` and `25/26/30`. The sampler reads two contiguous matrix spans, **3,552 bytes and 336 bytes**, rather than one request per joint. Each request remains below the transport's 4,096-byte ceiling. It is attempted only for admitted, projected characters inside the skeleton distance gate, and it publishes a complete bone set only after the chain and endpoints still match.

These indices were validated for the observed 231-bone character content; they are not a promise that every future asset or game build uses the same rig.

## Case study: Game Master activity

Finding an editor-manager entity did not prove somebody was using its camera. The entity and movement component persisted after leaving Game Master mode, and their last position and orientation remained readable. An inactive → active → inactive live transition identified an entity field at `+0x100` that changed `0 → 1 → 0`. The controller uses that freshly rechecked state before drawing the marker or view direction, so an inactive camera's frozen snapshot does not become a false positive.

The observed local transition validates the active-state behavior for that session. Whether the same entity represents another player's camera in a multiplayer session remains an open live-validation question.

## Turning evidence into a safe read path

The transport limits each read to 4,096 bytes. Scanner work refreshes entity membership at roughly one-second cadence; current camera and accepted entity positions are sampled at frame cadence. Optional features gate their reads before traversing their chains. Mutable roots and weak links are rechecked, and failed samples are discarded before the renderer sees them.

This makes the research useful beyond an offset table: each finding is accompanied by a read budget, a failure rule, and a way to reproduce its expected shape. The controller's renderer only draws the frame it was handed.

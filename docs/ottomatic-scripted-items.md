# Otto Matic scripted items

Otto Matic can create ports-only items backed by real engine `ObjNode` objects.
Definitions live in the active entry of `Data/Scripts/config/levels.json`; the
editor generates this file and packages referenced binary assets.

## Visuals

Supported visual kinds are:

- `nativeDisplayGroup`: an object from Otto's loaded `global` or
  `levelSpecific` BG3D group.
- `customDisplayGroup`: an object from a BG3D file below
  `Data/Scripts/assets/models/`.
- `nativeSkeleton`: an already-loaded Otto skeleton type and animation index.
- `customSkeleton`: a BG3D model plus a classic Mac skeleton resource below
  `Data/Scripts/assets/skeletons/`.

Custom skeleton manifests use the logical resource path ending in `.skeleton`.
The packaged resource-fork sidecar ends in `.skeleton.rsrc`. Animation maps give
scripts stable names while Otto continues to use validated native indexes.

The runtime reserves eight custom BG3D groups and four custom skeleton slots per
level. A file may be at most 16 MiB and all scripted assets together may be at
most 64 MiB. Paths outside `Data/Scripts/assets/`, traversal paths, missing
assets, invalid model indexes, and unknown animation names are rejected.

## Collision and lifecycle

Definitions can select `none`, `solidBox`, `triggerBox`, `pickup`, `enemy`, or
`platform`. Trigger and pickup presets use Otto's dedicated scripted trigger
handler rather than indexing an unrelated native trigger callback.

Custom behavior tables may implement:

- `onSpawn(self, ctx)`
- `onUpdate(self, ctx)`
- `onTriggerEnter(self, ctx)`
- `onAnimationEvent(self, ctx)`
- `onAnimationComplete(self, ctx)`
- `onDestroy(self, ctx)`

The object API exposes position, velocity, rotation, uniform scale, deletion,
and `setAnimation(handle, name, speed, blendSeconds)`. Handles are checked for
both identity and generation before every operation.

## Replacing native items

The editor can replace the selected terrain or spline item with any saved custom
object and restore it later. A rule records the stream indexes and a secondary
fingerprint (native type plus position or spline placement).

Replacement resolution runs before the native factory. The native item is marked
in use only after the custom object was successfully created. Missing definitions,
missing assets, exhausted custom slots, or invalid model/animation indexes fall
back to the untouched native factory. Strict failure is represented in the schema
for ports-only packages but the editor creates safe fallback rules by default.

Generic replacement reproduces the visual, script, transform, collision, and
lifecycle portions of an item. Bosses, progression controllers, vehicles, and
other objects coupled to global game state still require a reviewed Otto-specific
adapter; they must not be presented as exact generic replacements.

### Replacement catalog

The following classification is based on Otto's terrain factory table. “Generic”
means the item can be replaced before its factory runs and recreated with a
display-group or skeleton visual, a collision preset, and script lifecycle code.
It does not claim the original factory's private state was copied.

| Tier | Terrain types | Support |
|---|---|---|
| A: scenery/static | 1, 11–12, 14–25, 28, 32–34, 38, 42–43, 46–48, 56, 58, 63, 65–66, 68, 74, 77, 82, 85, 87, 91, 96–101, 106–108 | Generic display-group replacement |
| B: pickups/hazards/platforms/triggers | 5–6, 13, 19, 27, 29, 31, 36–37, 39, 44–45, 53–55, 57, 62, 64, 67, 70–71, 73, 75, 80, 84, 88, 98, 102, 104 | Generic replacement; scripts must reproduce scoring, damage, progression, or motion semantics |
| C: enemies/NPCs | 3–4, 7–10, 30, 49–52, 59–61, 78–79, 81, 89–95 | Custom/native skeleton replacement and named animations; AI and game-specific combat remain authored script behavior |
| D: coupled controllers | 2, 26, 41, 69, 72, 76, 83, 90, 103, 105 | Interception is safe, but exact replacement is not advertised without a dedicated adapter |

Spline items use the same fingerprint/fallback pipeline. The meaningful spline
factories—humans, common enemies, the magnet monster, moving platform, clown
fish, and rail gun—can be intercepted; their path-following semantics must be
implemented by the scripted copy. `NilAdd`/`NilPrime` entries have no native
factory behavior to preserve.

## Feature flag

When scripting is disabled, the editor does not mount scripting controls or
custom placement markers, and native builds compile out sidecar interception and
lifecycle dispatch.

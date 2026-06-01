# Otto Matic Scripting Extension Plan

## Goal

Extend Otto Matic scripting from level/item-spawn hooks into a practical gameplay customization layer that can attach behavior to live objects, starting with humans as the first vertical slice.

The immediate target use case is: a script can make all humans jump in place without editing native gameplay code for that behavior.

The broader target is shared consistency. Any lessons from Otto's implementation should be applied across all 8 Pangea Ports games:

- Billy Frontier
- Bugdom
- Bugdom 2
- Cro-Mag Rally
- Mighty Mike
- Nanosaur
- Nanosaur 2
- Otto Matic

## Current State

Otto Matic currently supports optional scripting behind `PANGEA_ENABLE_SCRIPTING`.

The integration can:

- Load `Data/Scripts/dist/main.js`, or a per-level script path from `Data/Scripts/config/levels.json`.
- Call level lifecycle hooks: `onLevelLoad`, `onLevelStart`, `onFrame`, `onLevelComplete`, `onLevelUnload`.
- Call terrain and spline item spawn hooks before native object creation.
- Remap terrain item types from level config.

The integration cannot yet:

- Attach a script behavior to a live `ObjNode`.
- Expose persistent object handles to scripts.
- Read or write live object position, velocity, animation, health, or deletion state.
- Spawn native objects from script despite the TypeScript API advertising `pangea.spawn.native`.
- Run per-object script update hooks.

This means scripts can currently observe or intercept a human terrain item before it becomes a human, but they cannot later animate that human.

## Design Principles

Prefer a shared object scripting model over Otto-only shortcuts. Otto can be the pilot, but the API shape should work for the other games.

Expose handles, not raw pointers. Native code should own object lifetime and validate handles on every script call.

Keep script authority narrow at first. Start with controlled reads/writes for position offsets or velocity rather than full `ObjNode` mutation.

Do not break native gameplay ownership. Script updates should compose with existing movement functions instead of replacing them by default.

Make missing scripts non-fatal. The current behavior is good for packaged games and editor workflows.

Keep TypeScript declarations honest. Do not advertise capabilities in `@pangea-ports/script-types` until the C backend and game adapters implement them.

## Proposed API Shape

### Object Handles

Add a shared native handle registry:

```ts
export interface ObjectHandle {
  readonly id: number;
  readonly generation: number;
}
```

Each registered native object gets a stable handle while alive. When deleted, its generation is invalidated so stale script handles fail safely.

### Object Access

Implement the already-declared object API incrementally:

```ts
pangea.object.position(handle): Vector3 | undefined
pangea.object.setPosition(handle, position): boolean
pangea.object.setVelocity(handle, velocity): boolean
pangea.object.delete(handle): boolean
```

For the first Otto slice, support:

- `position`
- `setPosition`
- optionally `setVelocity` if vertical jumping should use native physics

Defer health, damage, collision, and arbitrary object state until the handle model is proven.

### Object Tags

Add lightweight script tags for native objects. For Otto humans:

```ts
readonly tags: readonly ["ottomatic.human"]
```

Tags let scripts select groups of objects without relying on skeleton IDs, terrain item IDs, slots, or internal object type numbers.

### Per-Object Hooks

Add an object update hook that receives a handle and basic metadata:

```ts
export interface ObjectFrameContext extends FrameContext {
  readonly object: ObjectHandle;
  readonly tags: readonly string[];
  readonly position: Vector3;
}

export function onObjectFrame(ctx: ObjectFrameContext): void;
```

The shared backend should call this only for objects that a game adapter registers as script-visible.

## Otto Human Vertical Slice

### Native Registration

Register humans as script-visible when created by:

- `MakeHuman`
- `PrimeHuman`
- humans emitted from huts

Each human should be tagged as:

- `ottomatic.human`
- `ottomatic.human.farmer`, `ottomatic.human.beewoman`, `ottomatic.human.scientist`, or `ottomatic.human.skirtlady`

The handle should be removed or invalidated from the registry when the human is deleted, teleported away, or otherwise leaves gameplay.

### Update Hook

Call the shared object hook from `MoveHuman` after native state is loaded with `GetObjectInfo` and before final transform/bounding updates are committed.

The first implementation should allow a script to either:

- Set an absolute position through `pangea.object.setPosition`.
- Return a small transient render/movement offset.

The safer first version is a transient offset because it does not permanently fight terrain grounding or rescue logic:

```ts
export interface ObjectFrameResult {
  readonly positionOffset?: Vector3;
}
```

Otto can apply `positionOffset.y` after native human movement for visual jumping while leaving collision and rescue checks stable.

### Example Script

Target authoring experience:

```ts
import type { ObjectFrameContext, ObjectFrameResult } from "@pangea-ports/script-types";

export function onObjectFrame(ctx: ObjectFrameContext): ObjectFrameResult | void {
  if (!ctx.tags.includes("ottomatic.human")) {
    return;
  }

  return {
    positionOffset: {
      x: 0,
      y: Math.abs(Math.sin(ctx.levelTimeSeconds * 8)) * 80,
      z: 0,
    },
  };
}
```

If `Array.prototype.includes` is not available in the embedded JS target, the TypeScript build should downlevel this safely or examples should use an ES5-compatible helper.

## Backend Work

Implement object registry APIs in `shared/script`:

- Allocate object handles.
- Validate handle generation.
- Map handles back to native objects through game-provided callbacks.
- Invalidate handles on object deletion.

Add backend calls for:

- `pangea.object.position`
- `pangea.object.setPosition`
- `pangea.object.setVelocity`
- `onObjectFrame`

Keep the backend game-agnostic. It should not know what an Otto human is.

## Otto Adapter Work

Add Otto-specific callbacks that translate handle operations to `ObjNode` operations.

Register script-visible humans at creation sites.

Invalidate human handles on deletion. If there is not a single deletion hook available, add a small centralized helper and migrate only registered script-visible objects first.

Call `onObjectFrame` from human movement.

Ensure script offsets do not corrupt:

- terrain grounding
- collision bounds
- rescue trigger behavior
- saucer beam-up behavior
- teleport fade behavior
- humans in ice
- humans walking on splines

## TypeScript Package Work

Update `@pangea-ports/script-types` only after native support exists.

Add shared types for:

- `ObjectFrameContext`
- `ObjectFrameResult`
- object tags

Add Otto-specific types:

```ts
export type OttoMaticObjectTag =
  | "ottomatic.human"
  | "ottomatic.human.farmer"
  | "ottomatic.human.beewoman"
  | "ottomatic.human.scientist"
  | "ottomatic.human.skirtlady";
```

Update docs and examples to show both JavaScript and TypeScript usage.

## Level Editor Integration

The level editor should support scripting without changing the fundamental level data structures used by the original games.

Scripts, script config, item remaps, object tags, and editor-authored behavior bindings should be stored as sidecar data outside the original level files. The original terrain, map, spline, race, or area files should remain byte-compatible with the legacy games. A level edited with scripting metadata should still be usable in the original game if the sidecar script files are omitted.

Recommended layout:

```text
Data/Scripts/config/levels.json
Data/Scripts/dist/main.js
Data/Scripts/src/*.ts
```

The editor can present script behavior as attached to level items or object classes, but the saved representation should remain external metadata. For example, a human item in an Otto level can show an editor panel saying it participates in `ottomatic.human` script behavior, while the actual level item remains the normal native human item type. The script binding lives in `levels.json` or another `Data/Scripts/config/*.json` sidecar file.

The editor should avoid writing custom item types into legacy level files just to support scripting. If a behavior needs to replace native spawning, use sidecar `itemOverrides` or hook metadata interpreted only by the Pangea Ports runtime. When the same level is opened by an original game, the level file should contain only native item types and native parameters that the original game already understands.

Editor responsibilities:

- Validate sidecar script config with the shared TypeScript schemas before saving.
- Compile or bundle TypeScript into `Data/Scripts/dist/main.js` or a per-level script path.
- Show script diagnostics separately from level validation errors.
- Let authors enable, disable, or remove scripting sidecars without rewriting level geometry or item data.
- Preserve original level-file import/export fidelity when no native level edits were made.

Suggested level editor UI components:

- **Scripts panel**: Lists project scripts, compiled output status, active per-level script, and reload/test actions.
- **Hook browser**: Shows available hooks for the selected game, such as `onLevelStart`, `onFrame`, `onTerrainItem`, `onSplineItem`, and future `onObjectFrame`.
- **Behavior inspector**: Appears when a level item, spline item, or script-visible object class is selected. It shows attached script tags, behavior bindings, editable script parameters, and whether the binding is stored as sidecar metadata.
- **Script parameter editor**: Provides typed controls for behavior inputs, such as numbers, booleans, enums, colors, asset references, and vector values. These values should be saved in script sidecar config, not packed into unknown native item fields unless the original game already owns those fields.
- **Custom object palette**: Lets authors place scripting-defined objects that reference a behavior ID, model asset, collision preset, and default parameters.
- **Asset browser**: Tracks custom models, textures, animations, sounds, and scripts under a ports-only asset namespace.
- **Compatibility report**: Shows whether the current level is original-game-compatible, ports-only because it uses sidecar scripting, or incompatible because native level data was changed in a way the original game cannot understand.
- **Script console**: Displays runtime logs from `pangea.log`, build errors, and hook exceptions without mixing them into ordinary level validation.
- **Preview controls**: Runs the level in the Pangea Ports runtime with scripting enabled, with controls for reloading sidecar scripts without restarting the editor.

Recommended sidecar file types:

- `Data/Scripts/config/project.json`: project-level scripting manifest. It records schema version, enabled script features, build output paths, package metadata, and editor-only settings that should not be stored in native level files.
- `Data/Scripts/config/levels.json`: per-level, per-area, or per-track script config. It maps game locations to script entry points and runtime options.
- `Data/Scripts/config/bindings/<level-id>.json`: native item bindings. This attaches script behaviors to existing valid native items without changing native item type, position, or params.
- `Data/Scripts/config/placements/<level-id>.json`: ports-only custom object placements. These objects do not exist in the original level data and are spawned only by Pangea Ports.
- `Data/Scripts/config/objects.json`: custom object definition catalog. It defines reusable scripted object types, behavior IDs, tags, native-format model references, skeleton references, collision presets, default params, and editor preview metadata.
- `Data/Scripts/config/params.json`: optional behavior parameter schemas. The editor uses these schemas to render typed controls instead of raw JSON for script behavior settings.
- `Data/Scripts/src/**/*.ts`: editable TypeScript source owned by the editor/project.
- `Data/Scripts/dist/**/*.js`: compiled JavaScript loaded by the runtime.
- `Data/Scripts/assets/**`: ports-only script assets. Model assets here must still use the target game's native formats, such as `bg3d`, `qd3d`, and `Skeleton.rsrc` resources.
- `Data/Scripts/reports/compatibility.json`: optional editor-generated compatibility report. This can be regenerated and should not be required by the runtime.

The key split is mandatory: native item bindings and ports-only custom placements are different files with different semantics. Bindings decorate original native items. Placements create new ports-only objects outside the original level file.

The editor should validate all sidecar files through shared schemas and should be able to omit all sidecars for a purely original-game-compatible export.

Runtime responsibilities:

- Treat missing sidecar config or missing compiled scripts as non-fatal.
- Fall back to native behavior when no script is present.
- Load sidecar metadata by level number, scene/area, or race track depending on the game adapter.
- Keep all scripting-only semantics out of the original binary level formats.

This preserves a clean compatibility split: original games consume original data, while Pangea Ports can opt into richer behavior by loading adjacent script assets.

### Custom Scripted Objects

The editor should support custom objects as a ports-only layer without requiring changes to the original level file formats.

The proposed solution is: do not encode custom scripted objects as new native item types in the original level data.

Original level files already store item positions and enumerated native item types. The games usually dispatch those item types through fixed native add/prime tables and can crash when the type is outside the expected range. Therefore, custom scripted objects need a separate ports-only placement stream, not a fake native item enum value.

Use two sidecar concepts:

- **Native item bindings**: Attach scripts to existing native items without changing their native type or parameters.
- **Ports-only placements**: Define new scripted/custom objects in a sidecar file that only Pangea Ports reads.

Native item bindings reference existing level items by stable identity. The native item remains, for example, `MAP_ITEM_HUMAN = 4`; the sidecar says that this specific item, or all items with that type/tag, also participates in script behavior.

Example native binding:

```json
{
  "bindings": [
    {
      "target": {
        "kind": "terrainItem",
        "nativeType": 4,
        "index": 12
      },
      "tags": ["ottomatic.human", "example.jumpInPlace"],
      "behavior": "behaviors.jumpInPlace",
      "params": {
        "height": 80,
        "speed": 8
      }
    }
  ]
}
```

The `index` is the item's index in the original level item list. The editor can also store a secondary fingerprint, such as native type, position, and params, so it can warn if the level item list changes and the binding may need repair.

Ports-only placements define objects that do not exist in the original level file at all:

```json
{
  "customObjects": [
    {
      "id": "custom.bouncingRobot.001",
      "definition": "custom.bouncingRobot",
      "position": { "x": 1200, "y": 0, "z": -640 },
      "rotation": { "x": 0, "y": 1.57, "z": 0 },
      "params": {
        "height": 120,
        "speed": 4
      }
    }
  ],
  "definitions": [
    {
      "id": "custom.bouncingRobot",
      "behavior": "behaviors.bouncer",
      "model": "Data/Scripts/assets/models/bouncing-robot.bg3d",
      "collision": "capsule",
      "tags": ["custom.bouncingRobot"]
    }
  ]
}
```

The Pangea Ports runtime loads original level data first, then loads the sidecar custom object placements and spawns them through the script/custom-object system. The original game never sees those placements because they are not in the original level file.

For compatibility:

- A level with no custom scripted objects should export exactly like a normal original-game-compatible level.
- A level with custom scripted objects should keep the original level file usable by the original game, but the custom objects will be absent when sidecar files are omitted.
- The editor must not write out-of-range or custom enum values into native terrain, spline, map, race, or area item lists.
- If an author intentionally maps a behavior onto a native placeholder item, that placeholder must be a valid item type for the target game and must be expected to behave safely in the original game.
- Custom model files should live outside native art archives unless the project explicitly chooses a ports-only package format.
- The editor should show missing custom assets as sidecar errors, not as corruption of the base level.

Custom model support should use the target games' native asset formats. Do not introduce glTF/GLB or another unrelated runtime model format for game-side loading.

For Pangea games, that means custom objects should reference the same native model families the games already understand, such as `bg3d`, `qd3d`, and skeleton resources from `Skeleton.rsrc` where appropriate. The editor already has model parsers for these formats, so it should use those parsers for preview, validation, collision setup, and asset selection.

If the editor imports a third-party source model, conversion should happen before runtime packaging. The saved game-facing custom object definition should reference validated native-format assets, not the original interchange file.

The scripting API should treat custom objects the same way as native script-visible objects once spawned: they get object handles, tags, `onObjectFrame` hooks, and safe object operations. The difference is ownership: custom objects are created from sidecar metadata, while native objects are created from original level data.

## All-Games Follow-Through

After the Otto human vertical slice works, audit all 8 games and apply the design lessons consistently.

For each game, decide:

- Which native object classes should be script-visible first.
- Which tags describe those classes.
- Where object handles are registered and invalidated.
- Which movement/update functions should call object hooks.
- Which object operations are safe for scripts.

The expected outcome is one shared scripting model with game-specific adapters, not eight incompatible scripting systems.

## Rollout Steps

1. Fix the Duktape backend structure and add tests or a small build target that catches malformed backend code.
2. Add the shared object handle registry.
3. Bind read-only `pangea.object.position`.
4. Register Otto humans and expose `onObjectFrame` read-only.
5. Add transient `positionOffset` support for Otto humans.
6. Add `setPosition` only after offset behavior is stable.
7. Update TypeScript types and examples.
8. Document the Otto human jumping example.
9. Audit the other 7 games and create per-game adapter tasks.
10. Promote shared decisions into `typescript-scripting.md`, `typescript-api.md`, and `script-game-matrix.md`.

## Risks

Script mutation can fight native movement and collision. Prefer transient offsets first.

Object handles can become stale. Generation validation is mandatory.

Per-object hooks can be expensive. Only call them for registered script-visible objects, and keep hook contexts small.

Some games have different object lifetimes and map systems. The shared API should be stable, but registration and invalidation will be adapter-specific.

The current TypeScript API is ahead of the C backend. Tighten the declarations as implementation lands so examples do not imply unavailable behavior.

## Acceptance Criteria

Otto Matic can run a script that makes human characters visibly jump in place.

The script does not require native code changes for the behavior itself.

Humans still rescue, teleport, collide, and clean up correctly.

Missing scripts remain non-fatal.

Stale object handles fail safely.

The docs identify how the same object scripting model should be applied to all 8 games.

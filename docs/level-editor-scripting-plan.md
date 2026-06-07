# Level Editor Scripting Plan

## Goal

Add level-editor scripting support for all eight Pangea Ports games without
changing the fundamental level formats that the original games already load.

The editor should treat scripting as a ports-only augmentation layer:

- Original terrain, spline, map, race, and area data stay byte-compatible.
- Script metadata lives in sidecar files next to the original data.
- Omitting the sidecars should leave a valid original-game export.
- Pangea Ports can load the sidecars and opt into richer scripted behavior.

This plan applies across all eight current scripting adapters:

- Otto Matic
- Billy Frontier
- Bugdom
- Bugdom 2
- Cro-Mag Rally
- Mighty Mike
- Nanosaur
- Nanosaur 2

## Compatibility Contract

The editor must not write scripting-only concepts into native level files just
to make scripts work.

That means:

- No custom terrain item enums.
- No custom spline item enums.
- No custom map item enums.
- No custom race item enums.
- No ports-only metadata packed into unknown native fields unless the original
  game already owns and validates those fields.

Original games dispatch native item types through fixed tables and often assume
that incoming enum values are in range. Writing out-of-range values is not a
soft compatibility issue; it can crash the original game.

The editor therefore needs two separate concepts:

- Native item bindings: attach scripted behavior to existing valid native items.
- Ports-only placements: spawn new scripted objects from sidecar metadata only.

## Sidecar Layout

Recommended sidecar layout:

```text
Data/Scripts/config/project.json
Data/Scripts/config/levels.json
Data/Scripts/config/bindings/<level-id>.json
Data/Scripts/config/placements/<level-id>.json
Data/Scripts/config/objects.json
Data/Scripts/config/params.json
Data/Scripts/src/**/*.ts
Data/Scripts/dist/**/*.js
Data/Scripts/assets/**
Data/Scripts/reports/compatibility.json
```

Responsibilities for each file group:

- `project.json`: schema version, build output paths, enabled features, and
  editor-only project settings.
- `levels.json`: per-level, per-area, or per-track script entrypoints and
  runtime options.
- `bindings/<level-id>.json`: behavior attached to existing native items.
- `placements/<level-id>.json`: ports-only custom object placements.
- `objects.json`: reusable custom object definitions, tags, model references,
  collision presets, and default params.
- `params.json`: optional parameter schemas used to drive typed editor controls.
- `src/**/*.ts`: editable script source.
- `dist/**/*.js`: compiled output loaded by the runtime.
- `assets/**`: ports-only assets referenced by scripting sidecars.
- `reports/compatibility.json`: optional editor-generated compatibility report.

The important split is semantic, not cosmetic: bindings decorate original data;
placements create extra ports-only objects that the original game never sees.

## Editor Workflow

The editor should present scripting as first-class authoring data while still
keeping the saved representation external to the native level format.

Example: an Otto human can show a behavior panel that says it participates in
`ottomatic.human` scripting, but the underlying native item remains the same
original Otto human item type. The sidecar stores the behavior binding.

Editor responsibilities:

- Validate sidecar files against shared schemas before saving.
- Compile or bundle TypeScript into `Data/Scripts/dist/**/*.js`.
- Show script diagnostics separately from level-validation errors.
- Let authors remove or disable scripting without rewriting level geometry.
- Preserve exact import/export fidelity when no native level edits were made.

Suggested editor UI:

- Scripts panel: project scripts, output status, active entrypoint, reload, and
  preview actions.
- Hook browser: available hooks for the selected target game.
- Behavior inspector: tags, bindings, params, and sidecar ownership for the
  selected native or custom object.
- Script parameter editor: typed controls for numbers, booleans, enums, colors,
  vectors, and asset references.
- Custom object palette: reusable scripted object definitions for placement.
- Asset browser: ports-only scripts, models, textures, skeletons, sounds, and
  animations.
- Compatibility report: original-game-compatible, ports-only, or incompatible.
- Script console: build errors, `pangea.log` output, and runtime exceptions.
- Preview controls: run the Pangea Ports runtime, reload scripts, and retest.

## Native Item Bindings

Native item bindings attach behavior to real native items without changing the
item's type, position, or native parameters.

Example binding file:

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

The editor should store stable identity for the target item. The native index is
the first choice; a secondary fingerprint such as native type, position, and raw
params can help the editor detect drift when authors reorder or rebuild items.

## Ports-Only Custom Objects

Custom scripted objects should not be encoded as fake native item types.

Instead, store them in a ports-only placement file that only Pangea Ports loads
after the native level data is already in memory.

Example placement file:

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

Runtime flow:

- Load the original level data normally.
- Load scripting sidecars for the current level, area, or track.
- Spawn ports-only custom objects from the placement sidecar.
- Register both native and custom script-visible objects through the same object
  handle system.

This gives custom objects the same scripting surface as native objects once they
exist: tags, object handles, and `onObjectFrame`.

## Native Asset Rules

Custom scripted objects must use the games' native asset formats.

Do not introduce a separate runtime model format such as glTF or GLB for game
loading. The editor already understands the native families, and the runtime
already expects them.

Use the target game's existing native formats instead:

- `bg3d`
- `qd3d`
- skeleton resources from `Skeleton.rsrc`

If an artist imports an interchange format, conversion should happen before the
asset is saved for runtime use. The saved custom object definition should always
reference a validated native-format asset.

## Build And Validation Pipeline

The editor-side scripting pipeline should be explicit and reproducible:

- Author TypeScript under `Data/Scripts/src`.
- Validate sidecar JSON with shared schemas before save and before preview.
- Compile TypeScript into `Data/Scripts/dist`.
- Surface build failures in the script console.
- Keep missing scripts or sidecars non-fatal at runtime.

The runtime side should:

- Fall back to native behavior when sidecars are absent.
- Load sidecars by level number, scene/area, or track number depending on the
  target game.
- Keep script semantics out of the original binary formats.

## Game-Specific Mapping

The editor must map scripting sidecars onto each game's natural location model:

- Otto Matic, Bugdom, Bugdom 2, Nanosaur, and Nanosaur 2 use level numbers.
- Billy Frontier uses area numbers for its local modes.
- Cro-Mag Rally uses track numbers.
- Mighty Mike uses scene plus area.

The sidecar format can stay uniform as long as the editor resolves the correct
identity per game before save and preview.

## Acceptance Criteria

The level editor scripting work is complete when all of the following are true:

- All eight supported games can load script sidecars without native level-format
  changes.
- Native item bindings and custom placements are stored separately.
- The editor never writes out-of-range native item enums.
- Custom objects reference native-format assets only.
- Removing the scripting sidecars leaves a valid original-game export.
- The editor can preview, reload, validate, and report scripting issues without
  corrupting the base level data.# Level Editor Scripting Plan

## Goal

Extend the level editor so users can write scripts, assign them wherever the target game can safely support scripting, preview them in the updated Pangea Ports runtimes, and export either extended ports-only downloads or conservative original-game-compatible downloads.

The editor must not require changes to the original level data structures. Native level files should keep valid native item types, native item params, and native asset references. Scripting metadata should live in sidecar files.

## Compatibility Modes

### Conservative Download

A conservative download targets the original unmodified games.

It should include only original-compatible level data and original-compatible native assets. It should omit script sidecars, ports-only custom object placements, and any runtime-only script bundles.

If the edited level uses no ports-only features, this export should be byte-for-byte or structurally equivalent to the editor's normal level export.

If the edited project uses scripts, native item bindings, or custom object placements, the editor should clearly report that those features will be omitted from the conservative download. Existing native items remain in place because their types and params were never changed for scripting.

### Extended Download

An extended download targets the updated Pangea Ports runtimes.

It should include:

- Original-compatible level files.
- Script config sidecars.
- Native item binding sidecars.
- Ports-only custom object placement sidecars.
- Compiled JavaScript bundles.
- Native-format custom assets used by scripts or custom objects.

Extended downloads can add behavior and custom objects, but they should still keep the base level file usable by the original game if sidecars are removed.

## Code Editor

Use Monaco Editor as the embedded code editor. It is the editor component behind VS Code, has strong TypeScript/JavaScript support, and is a good fit for project-style script authoring.

Use a React wrapper such as `@monaco-editor/react` unless the project needs lower-level control over workers and bundling.

Required editor features:

- TypeScript and JavaScript syntax highlighting.
- Inline diagnostics.
- Autocomplete for `@pangea-ports/script-types`.
- Hover docs for game hooks and context objects.
- Multi-file editing.
- Basic project file tree.
- Format command.
- Find/replace.
- Error list tied to compiled output.

The editor should load generated `.d.ts` definitions for the selected game so scripts autocomplete only hooks and APIs that the selected game actually supports.

## Script Assignment Model

Scripts should be assignable wherever the runtime has a safe hook or can reasonably add one:

- Whole project.
- Level, area, scene, or race track.
- Terrain item type.
- Specific terrain item instance.
- Spline item type.
- Specific spline item instance.
- Native object class exposed by the game adapter.
- Specific native object instance, when a stable binding can be maintained.
- Ports-only custom object definition.
- Ports-only custom object placement.

Assignments should never be saved by changing native item enum values to custom values. The original games dispatch item types through fixed native tables and may crash on invalid values.

## Sidecar Files

Use sidecar files to store all scripting data:

- `Data/Scripts/config/project.json`: project scripting manifest, schema version, package metadata, compiler settings, and export defaults.
- `Data/Scripts/config/levels.json`: per-level, per-area, or per-track script entry points and runtime options.
- `Data/Scripts/config/bindings/<level-id>.json`: bindings from valid native items to script behaviors.
- `Data/Scripts/config/placements/<level-id>.json`: ports-only custom object placements.
- `Data/Scripts/config/objects.json`: reusable custom object definitions.
- `Data/Scripts/config/params.json`: parameter schemas for behavior inspector UI.
- `Data/Scripts/src/**/*.ts`: editable source files.
- `Data/Scripts/dist/**/*.js`: compiled runtime bundles.
- `Data/Scripts/assets/**`: ports-only assets in native game formats.

Native item bindings and ports-only placements must remain separate. Bindings decorate original native items. Placements create new objects that exist only in Pangea Ports.

## UI Components

### Scripts Workspace

Add a scripts workspace tab or panel with:

- File tree for `Data/Scripts/src`.
- Monaco editor.
- Build status.
- Script diagnostics.
- Runtime log console.
- Active game and level context.

### Hook Browser

Show available hooks for the selected game and selected target.

Examples:

- `onLevelLoad`
- `onLevelStart`
- `onFrame`
- `onTerrainItem`
- `onSplineItem`
- `onObjectFrame`
- custom object lifecycle hooks once implemented

Hooks that are only available in extended ports should be visually marked as extended-only.

### Assignment Inspector

When the user selects a level item, spline item, object class, or custom object placement, show a scripting section in the inspector.

It should show:

- Current native type and params.
- Whether the target is original-compatible.
- Assigned script behavior.
- Behavior tags.
- Typed behavior params.
- Binding source file and sidecar path.
- Extended-only warnings when applicable.

### Behavior Library

Provide a reusable behavior library with:

- Existing behaviors.
- Create behavior action.
- Duplicate behavior action.
- Behavior parameter schema.
- Supported target kinds.
- Extended/conservative compatibility status.

### Custom Object Palette

Add a palette for ports-only custom objects.

Each custom object definition should reference:

- Behavior ID.
- Tags.
- Native-format model asset, such as `bg3d`, `qd3d`, or `Skeleton.rsrc` resource.
- Collision preset.
- Default params.
- Optional editor icon or preview metadata.

The editor already has native model parsers, so custom object previews should use those parsers rather than introducing a new runtime model format.

### Compatibility Report

Add a compatibility report panel with two columns:

- Conservative original-game download.
- Extended Pangea Ports download.

For every script assignment or custom object, show whether it is included, omitted, or incompatible in each mode.

## Custom Objects

Custom scripted objects should be ports-only placements, not fake native item types.

The editor should store custom object placements in sidecars. The Pangea Ports runtime spawns them after loading the original level data. The original game never sees these objects because they are absent from the native level file.

Custom object assets should use the games' native formats. For Pangea games, that means `bg3d`, `qd3d`, and `Skeleton.rsrc`-based resources where appropriate.

If a project uses custom objects, the conservative download should omit those placements and report that the custom objects require the extended Pangea Ports runtime.

## Build And Preview

The editor should compile TypeScript to JavaScript before launching a preview.

Preview flow:

1. Save sidecar config in memory or a temporary workspace.
2. Compile scripts.
3. Upload original level files and sidecars into the Emscripten virtual filesystem.
4. Launch the selected Pangea Ports runtime.
5. Call scripting reload APIs when scripts change, where supported.
6. Stream `pangea.log` output into the script console.

Script build errors should block extended preview but should not corrupt or modify native level data.

## Runtime Requirements

Each game adapter should declare what is scriptable:

- Supported lifecycle hooks.
- Supported item binding kinds.
- Supported object tags.
- Supported object handle operations.
- Supported custom object features.
- Extended-only features.

The editor should consume this capability manifest instead of hard-coding assumptions per game.

## Download Flow

When downloading, present two clear actions:

- **Download Original-Compatible Level**: exports only native-compatible level data and native-compatible assets.
- **Download Extended Ports Package**: exports native-compatible level data plus scripts, sidecars, bundles, and ports-only assets.

If a user chooses the conservative download while extended features exist, show a concise summary of omitted features before export.

## Rollout

1. Define sidecar schemas with Zod and shared TypeScript types.
2. Add a scripting capability manifest per game.
3. Add Monaco-based script workspace.
4. Add script source and compiled bundle storage.
5. Add per-level script assignment.
6. Add native item binding assignment.
7. Add behavior parameter schemas and typed inspector controls.
8. Add extended preview with sidecar upload.
9. Add conservative and extended download flows.
10. Add custom object definitions and ports-only placements.
11. Add native-format asset selection for custom objects.
12. Apply the same model across all 8 games as each runtime adapter gains support.

## Acceptance Criteria

Users can write TypeScript scripts in the editor with game-aware autocomplete.

Users can assign scripts to all supported level, item, spline, native object, and custom object targets without modifying native item enum values.

The editor can preview extended scripts in the updated Pangea Ports runtimes.

The editor can export a conservative original-game-compatible download that omits scripts and ports-only custom objects.

Levels without extended features continue to work in the unmodified original games.

Extended downloads include all sidecars and native-format custom assets required by Pangea Ports.

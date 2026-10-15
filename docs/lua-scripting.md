<!-- BEGIN GENERATED SCRIPTING CONTRACT INDEX -->

This index is generated from `frontend/src/editor/subviews/scripts/scriptContract.ts`.

Hooks:
onDialogueResult onGameStart onGameShutdown onLevelLoad onLevelStart onLevelComplete onLevelUnload onSave
onLoad onFrame onTerrainItem onSplineItem onMapItem onObjectFrame onPickupCollected onWeaponHit
onTriggerEnter onDamage onDamageApplied onDeath onPlayerSpawn onPlayerRespawn onCheckpointReached onLapComplete
onRaceFinish onObjectiveComplete onAreaLoad onAreaStart onAreaFrame onAreaComplete onAreaUnload onRaceLoad
onRaceStart onRaceFrame onRaceComplete onRaceUnload

Events:
onTriggerEnter onPickupCollected onWeaponHit onDamage onDamageApplied onDeath onPlayerSpawn onPlayerRespawn
onCheckpointReached onLapComplete onRaceFinish onObjectiveComplete animationComplete destroy

Object events:
spawn update triggerEnter triggerStay triggerExit animationEvent animationComplete activate
deactivate streamIn streamOut checkpointReset destroy

Failure codes:
ok not-enabled file-not-found parse-error runtime-error bad-argument budget-exceeded incompatible-item
config-error

API functions:
pangea.ui.message pangea.ui.objective pangea.ui.remove pangea.ui.isActive pangea.dialogue.define pangea.dialogue.start pangea.dialogue.cancel pangea.dialogue.isActive
pangea.audio.play pangea.audio.stop pangea.audio.isActive pangea.effects.burst pangea.effects.attach pangea.effects.stop pangea.player.actions pangea.player.relations
pangea.player.carry pangea.player.drop pangea.player.handover pangea.player.mount pangea.player.dismount pangea.player.fire pangea.player.reload pangea.player.transform
pangea.player.respawn pangea.player.capture pangea.player.deliver pangea.player.resetCarried pangea.player.releaseBuddy pangea.player.grab pangea.player.interact pangea.player.commandBuddy
pangea.input.get pangea.world.sampleTerrain pangea.world.sampleTile pangea.world.categories pangea.world.overlapBox pangea.world.raycast pangea.world.sweepBox pangea.player.stateFields
pangea.player.getState pangea.player.setState pangea.level.complete pangea.level.rulesControlled pangea.level.setRulesControlled pangea.object.jointPosition pangea.object.attach pangea.object.detach
pangea.object.bounds pangea.object.moveKinematic pangea.object.scriptControlled pangea.object.setScriptControlled pangea.object.setScriptControlledResult pangea.object.setHealth pangea.object.setHealthResult pangea.object.setDamage
pangea.object.setDamageResult pangea.log.info pangea.log.warn pangea.log.error pangea.level.current pangea.level.setting pangea.api.capabilities pangea.api.diagnostics
pangea.api.requireVersion pangea.time.frame pangea.time.delta pangea.time.level pangea.time.after pangea.time.every pangea.time.cancel pangea.time.isActive
pangea.task.start pangea.task.wait pangea.task.cancel pangea.task.isActive pangea.events.on pangea.events.once pangea.events.off pangea.events.emit
pangea.random.number pangea.random.integer pangea.random.seed pangea.persistence.get pangea.persistence.set pangea.persistence.delete pangea.player.count pangea.player.get
pangea.player.raceResults pangea.player.objectiveResults pangea.player.checkpointResults pangea.player.setHealth pangea.player.setHealthResult pangea.player.setLives pangea.player.setLivesResult pangea.player.setScore
pangea.player.setScoreResult pangea.player.setWeaponQuantity pangea.player.setWeaponQuantityResult pangea.player.setKey pangea.player.setKeyResult pangea.player.setCloverCount pangea.player.setCloverCountResult pangea.player.setShieldActive
pangea.player.setShieldActiveResult pangea.player.heal pangea.player.healResult pangea.player.setInvulnerable pangea.player.setInvulnerableResult pangea.player.setPosition pangea.player.setPositionResult pangea.player.setVelocity
pangea.player.setVelocityResult pangea.player.setForm pangea.spawn.native pangea.spawn.nativeResult pangea.spawn.scripted pangea.object.position pangea.object.source pangea.object.velocity
pangea.object.rotation pangea.object.scale pangea.object.animation pangea.object.animationSpeed pangea.object.animationFrame pangea.object.active pangea.object.collisionEnabled pangea.object.all
pangea.object.findByTag pangea.object.nearest pangea.object.exists pangea.object.tags pangea.object.hasTag pangea.object.type pangea.object.category pangea.object.health
pangea.object.damage pangea.object.state pangea.object.captureCheckpoint pangea.object.restoreCheckpoint pangea.object.setPosition pangea.object.setPositionResult pangea.object.setPositionOffset pangea.object.setPositionOffsetResult
pangea.object.setVelocity pangea.object.setVelocityResult pangea.object.setRotation pangea.object.setRotationResult pangea.object.setScale pangea.object.setScaleResult pangea.object.setAnimation pangea.object.setAnimationResult
pangea.object.setCollisionEnabled pangea.object.setCollisionEnabledResult pangea.object.setActive pangea.object.setActiveResult pangea.object.delete pangea.object.deleteResult

API reference:
pangea.ui.message(text: string, options: table?): PresentationResult — Queues a bounded message in the native game HUD.
pangea.ui.objective(id: string, options: table): PresentationResult — Creates or updates a persistent named objective with optional progress.
pangea.ui.remove(handle: table): PresentationResult — Removes a generation-checked script presentation resource.
pangea.ui.isActive(handle: table): boolean — Checks whether a presentation resource is still active.
pangea.dialogue.define(id: string, nodes: table): PresentationResult — Defines up to sixteen dialogue nodes with named choices and optional native voices.
pangea.dialogue.start(id: string, options: table?): PresentationResult — Starts a nonmodal native conversation; completion and choice results reach onDialogueResult.
pangea.dialogue.cancel(handle: table): PresentationResult — Cancels a conversation and stops its owned voice.
pangea.dialogue.isActive(handle: table): boolean — Checks whether a conversation is active.
pangea.audio.play(id: string, options: table?): PresentationResult — Plays a named native sound; positional support is reported per game.
pangea.audio.stop(handle: table): PresentationResult — Stops a script-owned sound without stopping a native sound that reused its channel.
pangea.audio.isActive(handle: table): boolean — Checks whether an owned sound remains active.
pangea.effects.burst(id: string, options: table?): PresentationResult — Emits a bounded cosmetic native effect without gameplay damage.
pangea.effects.attach(id: string, options: table): PresentationResult — Creates an object-owned cosmetic emitter, released when its owner disappears.
pangea.effects.stop(handle: table): PresentationResult — Stops an attached cosmetic emitter.
pangea.player.actions(): string[] — Lists native player operation names supported by this game's adapter; an advertised action can still reject an incompatible actor or gameplay state.
pangea.player.relations(playerNum: number): PlayerRelations|nil — Reads live native carrying, mounting, buddy and grabbing relationships, form and capture count; invalid players return nil.
pangea.player.carry(playerNum: number, target: objectHandle): GameplayOperationResult — Establishes a native carrying relationship with a compatible live target.
pangea.player.drop(playerNum: number, options: table?): GameplayOperationResult — Releases the native carried object using supported position and velocity options.
pangea.player.handover(playerNum: number, target: objectHandle): GameplayOperationResult — Hands a carried object to a compatible native recipient.
pangea.player.mount(playerNum: number, target: objectHandle, options: table?): GameplayOperationResult — Enters a compatible native ride or vehicle using supported operation options.
pangea.player.dismount(playerNum: number): GameplayOperationResult — Leaves the player's native ride or vehicle with native relationship cleanup.
pangea.player.fire(playerNum: number, options: table?): GameplayOperationResult — Requests native weapon firing using supported aim, weapon and mode options.
pangea.player.reload(playerNum: number): GameplayOperationResult — Requests native weapon reload where the adapter supports it.
pangea.player.transform(playerNum: number, form: string, options: table?): GameplayOperationResult — Requests a supported native player form transition with relationship cleanup.
pangea.player.respawn(playerNum: number, options: table?): GameplayOperationResult — Requests native player respawn using supported position options or the game's native checkpoint rules.
pangea.player.capture(playerNum: number, target: objectHandle): GameplayOperationResult — Captures a compatible native target using the game's ownership rules.
pangea.player.deliver(playerNum: number, target: objectHandle?): GameplayOperationResult — Delivers the carried or captured object to an optional compatible native destination.
pangea.player.resetCarried(playerNum: number): GameplayOperationResult — Resets the native carried object and its ownership relationship.
pangea.player.releaseBuddy(playerNum: number): GameplayOperationResult — Releases the player's native buddy relationship.
pangea.player.grab(playerNum: number, target: objectHandle, options: table?): GameplayOperationResult — Establishes a native grabbing relationship with a compatible live target.
pangea.player.interact(playerNum: number, target: objectHandle, options: table?): GameplayOperationResult — Invokes a supported native interaction with a compatible live target.
pangea.player.commandBuddy(playerNum: number, mode: string, target: objectHandle?): GameplayOperationResult — Commands the player's native buddy using a supported mode and optional live target.
pangea.input.get(playerNum: number?): GameplayInput|nil — Reads semantic gameplay input for a player; unavailable input returns nil. Call during the frame before object simulation.
pangea.world.sampleTerrain(x: number, z: number): TerrainSample|nil — Samples native terrain height, normal and optional water height at world x/z without moving an actor.
pangea.world.sampleTile(x: number, y: number): TileSample|nil — Samples native tile type, flags and solidity at world x/y; unsupported games return nil.
pangea.world.categories(): string[] — Lists supported semantic collision category names for this game.
pangea.world.overlapBox(bounds: table, category: string): ObjectHandle[]|nil — Returns up to 64 live handles whose native collision bounds overlap a world box. Invalid categories or unsupported queries return nil.
pangea.world.raycast(from: vector3, to: vector3, category: string, ignore: objectHandle?): CollisionHit|nil — Finds the first registered native axis-aligned object box intersected by a world segment, optionally ignoring one object. Returns nil for no hit, invalid queries or broadphase saturation at 64 candidates. Excludes terrain, fences and unregistered geometry.
pangea.world.sweepBox(bounds: table, delta: vector3, category: string, ignore: objectHandle?): CollisionHit|nil — Sweeps a world box against registered native axis-aligned object boxes, optionally ignoring one object. Returns nil for no hit, invalid queries or broadphase saturation at 64 candidates. Excludes terrain, fences and unregistered geometry.
pangea.player.stateFields(): GameplayStateField[] — Lists native state fields, value domains, player scope and writable support for this game.
pangea.player.getState(playerNum: number, key: string): number|boolean|nil — Reads a named native gameplay state field. Unknown fields and invalid players return nil.
pangea.player.setState(playerNum: number, key: string, value: unknown): CommandResult — Writes a named native gameplay field within its advertised domain and returns structured status.
pangea.level.complete(playerNum: number, success: boolean): CommandResult — Requests native level or mode completion with a success outcome; unsupported boundaries return failure.
pangea.level.rulesControlled(): boolean — Checks whether Lua owns the adapter's automatic mode rules, separately from object movement controllers.
pangea.level.setRulesControlled(enabled: boolean): CommandResult — Claims or restores automatic mode rule ownership while preserving engine updates and explicit state/completion commands. Ownership resets on level load.
pangea.object.jointPosition(handle: objectHandle, joint: number): Vector3|nil — Reads a skeleton joint's world position; invalid joints and nonskeletal actors return nil.
pangea.object.attach(child: objectHandle, parent: objectHandle, options: table?): ObjectCommandResult — Attaches a child to a live parent origin by default, or a skeleton joint, with a world-space offset. Cycles and chains beyond 64 relationships are rejected.
pangea.object.detach(handle: objectHandle): ObjectCommandResult — Removes an object's script-owned attachment while preserving its current position.
pangea.object.bounds(handle: objectHandle): WorldBounds|nil — Reads a live object's world collision bounds; unavailable native bounds return nil.
pangea.object.moveKinematic(handle: objectHandle, delta: vector3, category: string): KinematicResult — Moves a script-controlled actor through at most three axis-aligned box sliding passes, returning position and contacts without native gameplay effects. Terrain, fence and unregistered static geometry response remains script-owned; saturated broadphase queries fail.
pangea.object.scriptControlled(handle: objectHandle): boolean — Checks whether Lua owns an object's movement controller. Native rendering and animation continue.
pangea.object.setScriptControlled(handle: objectHandle, enabled: boolean): boolean — Applies setScriptControlled through the native adapter; false indicates rejection or missing support.
pangea.object.setScriptControlledResult(handle: objectHandle, enabled: boolean): ObjectCommandResult — Applies setScriptControlled and returns structured diagnostics.
pangea.object.setHealth(handle: objectHandle, health: number): boolean — Applies setHealth through the native adapter; false indicates rejection or missing support.
pangea.object.setHealthResult(handle: objectHandle, health: number): ObjectCommandResult — Applies setHealth and returns structured diagnostics.
pangea.object.setDamage(handle: objectHandle, damage: number): boolean — Applies setDamage through the native adapter; false indicates rejection or missing support.
pangea.object.setDamageResult(handle: objectHandle, damage: number): ObjectCommandResult — Applies setDamage and returns structured diagnostics.
pangea.log.info(message: string): nil — Logs an informational message.
pangea.log.warn(message: string): nil — Logs a warning message.
pangea.log.error(message: string): nil — Logs an error message.
pangea.level.current(): number — Returns the current active level number.
pangea.level.setting(key: string): string|number|boolean|nil — Reads a typed setting from the active level configuration.
pangea.api.capabilities(): PangeaCapabilities — Reports runtime features available for the selected game.
pangea.api.diagnostics(): PangeaDiagnostics — Reports script memory, scheduler, subscription, frame, and deterministic command-stream diagnostics.
pangea.api.requireVersion(minimum: number, maximum: number?): true — Fails script loading unless the runtime API is within the requested range.
pangea.time.frame(): number — Returns the current frame number.
pangea.time.delta(): number — Returns the current frame delta in seconds.
pangea.time.level(): number — Returns elapsed level time in seconds.
pangea.time.after(delaySeconds: number, callback: function): number — Schedules a one-shot budgeted callback using level time.
pangea.time.every(intervalSeconds: number, callback: function): number — Schedules a drift-resistant repeating callback using level time.
pangea.time.cancel(timerId: number): boolean — Cancels a one-shot or repeating timer.
pangea.time.isActive(timerId: number): boolean — Checks whether a timer remains scheduled.
pangea.task.start(callback: function): number — Starts a budgeted coroutine task immediately.
pangea.task.wait(delaySeconds: number): nil — Suspends the current task for a level-time delay.
pangea.task.cancel(taskId: number): boolean — Cancels a suspended task.
pangea.task.isActive(taskId: number): boolean — Checks whether a task remains suspended or runnable.
pangea.events.on(eventName: string, callback: function): number — Subscribes to a script-local event.
pangea.events.once(eventName: string, callback: function): number — Subscribes for the next matching event emission only.
pangea.events.off(subscriptionId: number): boolean — Removes an event subscription.
pangea.events.emit(eventName: string, payload: unknown?): number — Synchronously emits an event with a read-only payload.
pangea.random.number(): number — Returns a deterministic number from zero through one.
pangea.random.integer(minimum: number, maximum: number): number — Returns a deterministic integer in an inclusive range.
pangea.random.seed(seed: number): nil — Resets the deterministic random stream.
pangea.persistence.get(key: string, version: number): string|number|boolean|nil — Reads a version-matched bounded persistent scalar, or nil when no value exists or the version does not match.
pangea.persistence.set(key: string, version: number, value: unknown): boolean — Stores a versioned bounded persistent scalar in the runtime persistence backend.
pangea.persistence.delete(key: string): boolean — Deletes a persistent value from the runtime persistence backend.
pangea.player.count(): number — Returns the number of active players exposed by the selected game.
pangea.player.get(playerNum: number): PangeaPlayerSnapshot|nil — Returns a normalized read-only player snapshot, including transform/aim, score/lives, fuel, token/coin/peso progress, child-object count, and structured weapon inventory where the selected game exposes those native fields.
pangea.player.raceResults(): PangeaRaceResult[]|nil — Returns the native read-only race result table, or nil when the selected game does not expose validated race state.
pangea.player.objectiveResults(): PangeaObjectiveResult[]|nil — Returns the bounded read-only objective result table observed from native objective completion events, or nil when no objective has completed.
pangea.player.checkpointResults(): PangeaCheckpointResult[]|nil — Returns the bounded read-only checkpoint result table observed from native checkpoint events, or nil when no checkpoint has been reached.
pangea.player.setHealth(playerNum: number, health: number): boolean — Sets a player's normalized health when the native adapter exposes a safe health mutation boundary.
pangea.player.setHealthResult(playerNum: number, health: number): PlayerCommandResult — Sets normalized player health and returns structured status and diagnostics.
pangea.player.setLives(playerNum: number, lives: number): boolean — Sets a player's lives count when the native adapter exposes a safe lives mutation boundary.
pangea.player.setLivesResult(playerNum: number, lives: number): PlayerCommandResult — Sets a player's lives count and returns structured status and diagnostics.
pangea.player.setScore(playerNum: number, score: number): boolean — Sets a player's score when the native adapter exposes a safe score mutation boundary.
pangea.player.setScoreResult(playerNum: number, score: number): PlayerCommandResult — Sets a player's score and returns structured status and diagnostics.
pangea.player.setWeaponQuantity(playerNum: number, weaponType: number, quantity: number): boolean — Sets a player's bounded weapon quantity when the native adapter exposes safe inventory mutation.
pangea.player.setWeaponQuantityResult(playerNum: number, weaponType: number, quantity: number): PlayerCommandResult — Sets a player's bounded weapon quantity and returns structured status and diagnostics.
pangea.player.setKey(playerNum: number, keyId: number, enabled: boolean): boolean — Sets a player's semantic key inventory state when the native adapter exposes safe key mutation.
pangea.player.setKeyResult(playerNum: number, keyId: number, enabled: boolean): PlayerCommandResult — Sets a player's semantic key inventory state and returns structured status and diagnostics.
pangea.player.setCloverCount(playerNum: number, color: stringUnion, count: number): boolean — Sets a player's named clover count when the native adapter exposes safe collectible mutation.
pangea.player.setCloverCountResult(playerNum: number, color: stringUnion, count: number): PlayerCommandResult — Sets a player's named clover count and returns structured status and diagnostics.
pangea.player.setShieldActive(playerNum: number, active: boolean): boolean — Activates or deactivates a player's native shield when the adapter exposes that boundary.
pangea.player.setShieldActiveResult(playerNum: number, active: boolean): PlayerCommandResult — Activates or deactivates a player's native shield and returns structured status and diagnostics.
pangea.player.heal(playerNum: number, amount: number): boolean — Adds normalized health to a player, clamped to full health, when the native adapter exposes safe health read and mutation boundaries.
pangea.player.healResult(playerNum: number, amount: number): PlayerCommandResult — Heals a player and returns structured status and diagnostics.
pangea.player.setInvulnerable(playerNum: number, durationSeconds: number): boolean — Sets a player's native invulnerability timer in seconds; zero disables it.
pangea.player.setInvulnerableResult(playerNum: number, durationSeconds: number): PlayerCommandResult — Sets player invulnerability and returns structured status and diagnostics.
pangea.player.setPosition(playerNum: number, position: vector3): boolean — Teleports a player when the native adapter exposes a safe position mutation boundary.
pangea.player.setPositionResult(playerNum: number, position: vector3): PlayerCommandResult — Teleports a player and returns structured status and diagnostics.
pangea.player.setVelocity(playerNum: number, velocity: vector3): boolean — Sets a player's velocity when the native adapter exposes a safe velocity mutation boundary.
pangea.player.setVelocityResult(playerNum: number, velocity: vector3): PlayerCommandResult — Sets a player's velocity and returns structured status and diagnostics.
pangea.player.setForm(playerNum: number, form: stringUnion): boolean — Changes a supported player's native playable form, such as Bugdom's bug and ball forms.
pangea.spawn.native(id: string, position: vector3, options: stringUnion?): ObjectHandle|nil — Spawns a native object.
pangea.spawn.nativeResult(id: unknown, position: vector3, options: table?): NativeSpawnResult — Spawns a native object and returns structured status and diagnostics.
pangea.spawn.scripted(id: string, position: vector3, options: stringUnion?): ObjectHandle|nil — Spawns a custom scripted object.
pangea.object.position(handle: objectHandle): Vector3|nil — Gets the position of an object.
pangea.object.source(handle: objectHandle): ObjectSource|nil — Gets the validated terrain, spline, or map source record for a replacement object.
pangea.object.velocity(handle: objectHandle): Vector3|nil — Gets the current native or scripted velocity of an object when the adapter exposes it.
pangea.object.rotation(handle: objectHandle): Vector3|nil — Gets the current native or scripted rotation of an object when the adapter exposes it.
pangea.object.scale(handle: objectHandle): number|nil — Gets the current uniform native or scripted scale of an object when the adapter exposes it.
pangea.object.animation(handle: objectHandle): number|nil — Gets the current native or scripted animation index of an object when the adapter exposes it.
pangea.object.animationSpeed(handle: objectHandle): number|nil — Gets the current native or scripted animation speed of an object when the adapter exposes it.
pangea.object.animationFrame(handle: objectHandle): number|nil — Gets the current native animation frame of an object when the adapter exposes it.
pangea.object.active(handle: objectHandle): boolean|nil — Gets the current active lifecycle state of an object when the adapter exposes it.
pangea.object.collisionEnabled(handle: objectHandle): boolean|nil — Gets the current native or scripted collision state of an object when the adapter exposes it.
pangea.object.all(): ObjectHandle[] — Returns all currently registered object handles.
pangea.object.findByTag(tag: string): ObjectHandle[] — Returns registered object handles carrying a tag.
pangea.object.nearest(origin: vector3, tag: string?): ObjectHandle|nil — Returns the nearest readable registered object, optionally filtered by tag.
pangea.object.exists(handle: objectHandle): boolean — Checks whether a generation-checked object handle is live.
pangea.object.tags(handle: objectHandle): string[] — Returns the tags assigned to an object.
pangea.object.hasTag(handle: objectHandle, tag: string): boolean — Checks whether an object has a tag.
pangea.object.type(handle: objectHandle): string|nil — Returns the native registration type of an object, such as an enemy, pickup, hazard, or scripted object.
pangea.object.category(handle: objectHandle): string|nil — Returns the semantic category of a live native or scripted object, such as enemy, pickup, hazard, projectile/effect, or scripted.
pangea.object.health(handle: objectHandle): number|nil — Returns native object health when the adapter exposes it, without granting mutation authority.
pangea.object.damage(handle: objectHandle): number|nil — Returns native object damage contribution when the adapter exposes it, without granting mutation authority.
pangea.object.state(handle: objectHandle): table|nil — Returns mutable script-owned state scoped to an object generation.
pangea.object.captureCheckpoint(handle: objectHandle): boolean — Captures a bounded deep copy of script-owned object state for checkpoint/reset restoration.
pangea.object.restoreCheckpoint(handle: objectHandle): boolean — Restores the object's script-owned state from its captured checkpoint baseline.
pangea.object.setPosition(handle: objectHandle, position: vector3): boolean — Sets the position of an object.
pangea.object.setPositionResult(handle: objectHandle, position: vector3): ObjectCommandResult — Sets an object's position and returns structured status and diagnostics.
pangea.object.setPositionOffset(handle: objectHandle, offset: vector3): boolean — Sets the current frame's visual position offset for an object. Only valid during onObjectFrame.
pangea.object.setPositionOffsetResult(handle: objectHandle, offset: vector3): ObjectCommandResult — Sets the current frame's visual position offset and returns structured status and diagnostics.
pangea.object.setVelocity(handle: objectHandle, velocity: vector3): boolean — Sets the velocity of an object.
pangea.object.setVelocityResult(handle: objectHandle, velocity: vector3): ObjectCommandResult — Sets an object's velocity and returns structured status and diagnostics.
pangea.object.setRotation(handle: objectHandle, rotation: vector3): boolean — Sets an object's Euler rotation.
pangea.object.setRotationResult(handle: objectHandle, rotation: vector3): ObjectCommandResult — Sets an object's Euler rotation and returns structured status and diagnostics.
pangea.object.setScale(handle: objectHandle, scale: number): boolean — Sets an object's uniform scale.
pangea.object.setScaleResult(handle: objectHandle, scale: number): ObjectCommandResult — Sets an object's uniform scale and returns structured status and diagnostics.
pangea.object.setAnimation(handle: objectHandle, animation: unknown, speed: number?, blendSeconds: number?): boolean — Sets an object's animation by name or numeric ID.
pangea.object.setAnimationResult(handle: objectHandle, animation: unknown, speed: number?, blendSeconds: number?): ObjectCommandResult — Sets an object's animation and returns structured status and diagnostics.
pangea.object.setCollisionEnabled(handle: objectHandle, enabled: boolean): boolean — Enables or disables an object's native collision checks.
pangea.object.setCollisionEnabledResult(handle: objectHandle, enabled: boolean): ObjectCommandResult — Enables or disables native collision checks and returns structured status and diagnostics.
pangea.object.setActive(handle: objectHandle, active: boolean): boolean — Activates or deactivates an object through a validated enabled-state transition.
pangea.object.setActiveResult(handle: objectHandle, active: boolean): ObjectCommandResult — Activates or deactivates an object and returns structured status and diagnostics.
pangea.object.delete(handle: objectHandle): boolean — Deletes an object.
pangea.object.deleteResult(handle: objectHandle): ObjectCommandResult — Deletes an object and returns structured status and diagnostics.

Result shapes:
ItemSpawnResult PickupResult WeaponHitResult TriggerResult DamageResult PresentationResult GameplayOperationResult CommandResult
ObjectCommandResult KinematicResult PangeaCapabilities PangeaDiagnostics PangeaPlayerSnapshot PangeaRaceResult PangeaObjectiveResult PangeaCheckpointResult
PlayerCommandResult NativeSpawnResult

Game adapters:
OttoMatic-Android Bugdom-android Bugdom2-Android Nanosaur-android Nanosaur2-Android CroMagRally-Android BillyFrontier-Android MightyMike-Android

<!-- END GENERATED SCRIPTING CONTRACT INDEX -->

## Custom runtime assets

Script packages may include native assets only under `Data/Scripts/assets/`.
The editor rejects traversal, absolute, and unsafe filenames, and each asset is
limited to 16 MiB.

- BG3D models are supported by the 3D adapters that advertise custom BG3D
  assets. Geometry material references and triangle indices must be valid;
  the native `-1` no-material sentinel is allowed. Textures must have positive
  integer dimensions and a pixel buffer whose length matches its declaration.
- Bugdom and Nanosaur use 3DMF model assets. Their native model loader paths
  are validated separately from BG3D and do not accept a BG3D substitute.
- Mighty Mike uses Shapes assets and does not accept BG3D or 3DMF models.
- Skeletal objects require a paired model and `.skeleton` resource fork. The
  model, joint table, limb data, animation table, and named animation mapping
  must be compatible. The resource fork is packaged as the sibling
  `<name>.skeleton.rsrc` file while the script refers to
  `<name>.skeleton`.
- glTF uploads are normalized and converted to BG3D for adapters that support
  BG3D. The normalized source is retained in the package; conversion output is
  reparsed and loaded by the supported generated runtimes before acceptance.

Preview VFS files and exported script-package files are generated from the
same validated byte records. Invalid or missing native assets return a typed
incompatible-item result and are cleaned from the adapter registry before the
next load attempt.

## Game presentation

All eight adapters support bounded HUD messages, objectives, nonmodal branching
dialogue, owned sound handles and cosmetic particle effects. Inspect
`pangea.api.capabilities().presentation.sounds` and `.effects` for the native
names available in the current game. Missing assets and unsupported operations
return a presentation result with `ok=false`; check that result before storing
its handle.

| Game | Presentation integration |
| --- | --- |
| Otto Matic | HUD-scale-aware panel; eight sounds; sparkles, smoke, coloured sparks, snow, bubbles and teleport glow. |
| Bugdom | Cached text with shadows, font readiness and visible missing-glyph fallback. |
| Bugdom 2 | Script dialogue suspends native dialogue and speech, preserving pending quest messages. Interrupted native recordings restart afterward. |
| Cro-Mag Rally | Separate conversations and row budgets for up to four active local players; six main-bank sounds; pane-relative text. |
| Nanosaur | Safe growing text meshes, accented-letter fallback and placement outside native HUD bars. |
| Nanosaur 2 | Separate conversations and row budgets for two active players, with each player's input and HUD placement. |
| Billy Frontier | Ten sounds; six cosmetic effect presets; width-fitted text with backing and shadows. |
| Mighty Mike | Relative sound volume/pitch without replacing busy native channels; particle lifetime follows simulation time and freezes while paused. |

Dialogue defaults to player zero. Pass `{player=1}` to target the second active
local player; a player already in a conversation returns `capacity-exceeded`.
Other players can start independent conversations. UI messages and objectives
default to every viewport. `onDialogueResult(ctx)` supplies `playerNum`,
`dialogueId`, `choiceId`, `reason` and the conversation handle, so results can be
associated with the correct player. Conversations targeting players that leave
are cancelled on the next update.

```lua
local pangea = require("pangea")
return {
  onLevelStart = function()
    local result = pangea.dialogue.define("greeting", {
      {speaker="Guide", text="Ready to explore?", duration=0,
       choices={{id="ready", text="Yes", next=0}}},
    })
    if result.ok then pangea.dialogue.start("greeting", {player=0}) end
  end,
  onDialogueResult = function(ctx)
    if ctx.reason == "choice" and ctx.choiceId == "ready" then
      pangea.ui.message("Good luck!", {player=ctx.playerNum})
    end
  end,
}
```

Use the game's area or race start hook instead of `onLevelStart` where required.
The editor's dialogue line sound selector exposes that game's named sounds;
custom recorded voice assets, portraits, modal gameplay suspension, looping
audio and music fades remain unsupported. Legacy fonts use fallback characters
for glyphs absent from their bundled atlas. Cro-Mag world effects retain native
camera visibility rather than being restricted to one player's pane.

## Gameplay services and controller ownership

The gameplay APIs expose engine services without requiring a native enemy or
player behaviour to implement each scripted rule. Their availability and data
domains vary by game and mode. Inspect `pangea.api.capabilities().gameplay`,
`pangea.world.categories()` and `pangea.player.stateFields()` at runtime rather
than assuming that another game's state keys or collision categories exist.
Player indices are zero-based integers.

`pangea.input.get(playerNum)` returns movement/look axes and an `actions` table
whose entries contain `held`, `pressed` and `released`. Action names are
game-specific. `aim` is optional. Read input from the game's frame hook before
object simulation; unavailable input returns `nil`.

Controller ownership is explicit:

```lua
local pangea = require("pangea")
local function takeControl(handle)
  local result = pangea.object.setScriptControlledResult(handle, true)
  if not result.ok then
    pangea.log.warn(result.message)
  end
  return result.ok
end
```

Taking control suppresses an eligible object's native movement controller while
retaining engine rendering and animation. It does not transfer every game rule
to Lua or make a native constructor independent of its native assets and setup.
Use `setScriptControlledResult(handle, false)` to restore native movement;
detach script-owned relationships first. Inactive objects and controller
ownership are separate states.

`pangea.object.delete` rejects native player bodies before delivering a destroy
callback. Removing a player requires a game-specific despawn operation; general
object deletion remains available for eligible ordinary and custom actors.

`pangea.level.setRulesControlled(true)` separately claims the adapter's automatic
mode rules. `pangea.level.rulesControlled()` reports that state. This selectively
suppresses the native automatic rule decisions covered by each adapter while
preserving engine updates and explicit state/completion commands. It does not
claim object movement controllers or pause simulation. Ownership resets on
level load; scripts must claim it again for each level and implement the rules
they suppress. Pass `false` to restore native automatic rule handling.

`pangea.object.bounds(handle)` supplies world-space `min`/`max` vectors.
`overlapBox`, `raycast` and `sweepBox` operate on registered native object
collision boxes. Ray and sweep tests do not include terrain, fences or
unregistered geometry, and return `nil` for no hit or an invalid/incomplete
query. They use a bounded broadphase with at most 64 candidates.
`moveKinematic` requires controller ownership and performs at most three box
sliding passes, returning `ok`, `position` and `contacts`. It does not apply
native damage, pickup or trigger consequences. Scripts must implement those
rules and handle terrain or tile response separately.

`sampleTerrain(x, z)` provides height, normal, surface type and optional water
height where supported. Mighty Mike's 2D world instead exposes
`sampleTile(x, y)` with native tile type, flags and solidity. Neither sampling
API is a complete native character collision solver.

State discovery returns each field's scalar type, writable flag, numeric domain
and player scope. Read-only or unavailable fields cannot be changed through
`setState`. The writable flag describes potential support; a particular write
may also require rule ownership or satisfy coupled native state invariants.
Check its `CommandResult` rather than treating the descriptor as unconditional
permission. `level.complete(playerNum, success)` requests a supported native
completion outcome; it does not replace the game's entire progression system.
Check the structured result of every mutation. `setHealth` and `setDamage`
require finite nonnegative values; writing health alone is not a full combat or
death transaction.

Attachments accept a parent origin or supported skeleton joint and a
world-space offset. Cycles and chains beyond 64 relationships are rejected.
Detaching preserves the current position. These relationships provide placement
and cleanup; they do not automatically reproduce native carrying, inventory or
handover rules.

Use `pangea.player.actions()` to discover native relationship and weapon
operations supported by the current game. These operation names are distinct
from the held/pressed/released input actions. `pangea.player.relations(playerNum)`
reports live carried, mounted, buddy and grabbed handles, the native form and
the capture count. Absent relationships are `nil`; handles may become stale
when an actor is deleted or the player changes level.

The operation methods return `GameplayOperationResult`, containing `ok`,
`code`, `reason`, `message` and an optional affected `object`. An advertised
operation can reject an incompatible target, unavailable ammunition or an
invalid native state. Check its result before updating script-owned objectives.
Native operations preserve the game's carrying, mounting, capture and weapon
rules rather than merely moving objects into place.

```lua
local function dropCarried(playerNum, velocity)
  local relations = pangea.player.relations(playerNum)
  if not relations or not relations.carried then return false end
  local result = pangea.player.drop(playerNum, {velocity=velocity})
  if not result.ok then pangea.log.warn(result.message) end
  return result.ok
end
```

Only pass options supported by the particular operation and game. The shared
options vocabulary includes `position`, `velocity`, `aim`, `weapon`, `joint`,
`mode` and a live `target`. `transform` accepts a form name plus optional
options, while `commandBuddy` accepts a mode and optional target. Unsupported
options must be rejected rather than treated as successful no-ops. As with
other gameplay mutations, these operations are disabled in network sessions.

Native action semantics differ between games:

- **Otto Matic:** `carry` performs native powerup pickup; ordinary powerups
  deposit into inventory, while a magnet can remain held. `drop`/`resetCarried`
  cancel pickup or release the magnet; in flying-saucer mode they recreate held
  humans on the ground. `capture` synchronously commits a native human into
  saucer inventory and deletes its original handle. `deliver` starts native rocket
  beam-down and requires an exit rocket. `mount` accepts a bumper car or ice
  saucer; `dismount` jumps out of a bumper car or leaves an ice saucer before
  completion, but cannot leave the flying-saucer player body. `fire` optionally
  selects a native weapon and uses native aim and inventory. `respawn` resets
  a dead robot at its checkpoint. Script-owned controllers temporarily delegate
  asynchronous native pickup, firing and ice-boarding behaviour for at most
  15 seconds, or delivery for at most 120 seconds, without revoking shared
  ownership. Other operation options and reload/transform/handover are unsupported.
- **Bugdom:** `mount` accepts native waterbugs or dragonflies and honours the
  waterbug fare. `grab` accepts a native root-swing skeleton and optional
  `joint` (default zero). `drop`/`dismount` release the root or ride. `transform`
  accepts `bug` or `ball`; ball time and headroom restrictions still apply and
  the returned replacement body handle supersedes the old handle.
  `commandBuddy` accepts `follow` or `attack`, with an optional native buddy
  target; `releaseBuddy` dismisses the current companion. `respawn` requires a
  dead player and uses the native checkpoint reset.
- **Bugdom 2:** `carry` requires an eligible visible native pickup within 300
  world units; powerups and can tabs are excluded. `drop` invokes the item's
  native drop callback. `handover` and `interact` require a native trigger
  within 300 units; handover succeeds only when it takes the held item.
  `mount` accepts a ride ball or balsaplane; `transform` with form `plane`
  requires the Balsa level, while form `bug` leaves the plane. `fire` accepts
  `mode="bullet"` or `"bomb"` while flying the plane. Buddy commands accept
  `recruit` or `launch`; `releaseBuddy` launches one following buddy. These
  operations require a living terrain player and are unavailable in tunnels.
  Script-owned riders retain ball/plane pose alignment and held-object
  placement without running native steering or flight logic. The mounted
  vehicle's native controller is also suppressed while its rider is script-owned;
  scripts must drive that vehicle explicitly.
- **Nanosaur:** `carry` establishes the native egg-mouth relationship;
  `drop` accepts a world `velocity`, and `resetCarried` restores the egg's
  original spawn. `deliver` requires an egg-recovery portal and commits score
  and species progress once; exit portals are different targets. `interact`
  starts native exit behaviour within 60 units on x/z. Its explicit transition
  service advances exit movement without processing native gameplay input,
  including for a script-owned player, and cancels after eight seconds if the
  transition has not finished. `fire` accepts native
  `weapon` indices 0–4 and retains ammunition and rate limits. `respawn`
  requires death and a spare life and consumes a life through native reset.
- **Nanosaur 2:** `carry` accepts a live native egg, `handover` transfers it to
  another empty-handed living player, and `drop` optionally supplies velocity
  and delays automatic pickup for one second. `resetCarried` returns it to its
  nest. `deliver` commits egg progress immediately at a matching nearby egg
  wormhole, without the cinematic; the destination is optional when discoverable.
  `fire` accepts an optional weapon and `mode="press"`, `"hold"` or
  `"release"`; Sonic Scream charges on press/hold and fires on release.
  `respawn` uses the best native checkpoint and consumes a life only under
  native rules. With scripted rules, scripts also own automatic egg pickup,
  reset, victory and extraction-opening decisions.
  A script-owned carried egg still aligns to its carrier through the native
  attachment service; controller ownership does not sever the carrying pose.
- **Cro-Mag Rally:** in capture-the-flag mode, `carry`/`capture` accept a native
  team torch; `handover` accepts another living local player's car, and
  `deliver` requires the matching team base and updates native scoring.
  `drop` accepts `position`/`velocity`; `resetCarried` returns the torch to its
  spawn. `fire` accepts the current powerup `weapon` and `mode="forward"` or
  `"backward"` and consumes inventory immediately. `respawn` accepts position
  and velocity, clears transient driving damage/control state and preserves
  lap, checkpoint and inventory progress. Players already are vehicles;
  mount/dismount are not advertised.
- **Billy Frontier:** shootout and target-practice `fire` optionally accepts a
  nonzero world `aim` direction; otherwise it uses crosshairs. Target practice
  has no player body. Duel fire accepts `mode="forward"`, `"left"` or
  `"right"` (or weapon 0/1/2), requires the matching draw animation and live
  target, and rejects custom aim. `transform` selects supported mode-specific
  animations; a native controller can change them again unless script-owned.
  Shootout `reload` clears the pending reload and plays its cue; it does not
  refill inventory. `respawn` revives an existing body in place, preserving
  inventory and ownership, and rejects target practice or committed completion.
- **Mighty Mike:** `fire` accepts optional weapon and planar aim (`z=0`),
  quantized to eight directions, retaining native ammunition, cooldown and
  wall-spawn checks. `transform` accepts `mike`, `frog` or `ship`; frog needs
  loaded Fairy assets and ship needs Bargain assets. Return to Mike before
  changing frog to ship. A ship transform can accept a native ship `target`,
  or create a ship body; `mount` accepts a native ship and `dismount` restores
  Mike. Body swaps invalidate the old handle and return the new one, preserving
  script controller ownership. `respawn` restores Mike, health and native
  invulnerability at an optional in-bounds planar position or the last safe
  spot, without consuming a life. Writable `formSeconds` controls current
  frog/ship duration; there is no native reload mechanic.

These services expand gameplay coverage but do not establish complete scripted
replacement of any game's gameplay. Native mode rules, specialised geometry,
combat side effects, save reconstruction and multiplayer authority still need
individual coverage. Gameplay mutation is disabled in network sessions;
local split-screen is distinct from networking.

The Lua sandbox does not expose `__gc` finalizers. Use explicit object lifecycle
hooks and owner-bound resources for cleanup, and cancel unowned timers, tasks,
subscriptions and presentation resources when their gameplay scope ends.

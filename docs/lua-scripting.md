<!-- BEGIN GENERATED SCRIPTING CONTRACT INDEX -->

This index is generated from `frontend/src/editor/subviews/scripts/scriptContract.ts`.

Hooks:
onGameStart onGameShutdown onLevelLoad onLevelStart onLevelComplete onLevelUnload onSave onLoad
onFrame onTerrainItem onSplineItem onMapItem onObjectFrame onPickupCollected onWeaponHit onTriggerEnter
onDamage onDamageApplied onDeath onPlayerSpawn onPlayerRespawn onCheckpointReached onLapComplete onRaceFinish
onObjectiveComplete onAreaLoad onAreaStart onAreaFrame onAreaComplete onAreaUnload onRaceLoad onRaceStart
onRaceFrame onRaceComplete onRaceUnload

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
pangea.log.info pangea.log.warn pangea.log.error pangea.level.current pangea.level.setting pangea.api.capabilities pangea.api.diagnostics pangea.api.requireVersion
pangea.time.frame pangea.time.delta pangea.time.level pangea.time.after pangea.time.every pangea.time.cancel pangea.time.isActive pangea.task.start
pangea.task.wait pangea.task.cancel pangea.task.isActive pangea.events.on pangea.events.once pangea.events.off pangea.events.emit pangea.random.number
pangea.random.integer pangea.random.seed pangea.persistence.get pangea.persistence.set pangea.persistence.delete pangea.player.count pangea.player.get pangea.player.raceResults
pangea.player.objectiveResults pangea.player.setHealth pangea.player.setHealthResult pangea.player.heal pangea.player.healResult pangea.player.setInvulnerable pangea.player.setInvulnerableResult pangea.player.setPosition
pangea.player.setPositionResult pangea.player.setVelocity pangea.player.setVelocityResult pangea.spawn.native pangea.spawn.nativeResult pangea.spawn.scripted pangea.object.position pangea.object.source
pangea.object.all pangea.object.findByTag pangea.object.nearest pangea.object.exists pangea.object.tags pangea.object.hasTag pangea.object.state pangea.object.setPosition
pangea.object.setPositionResult pangea.object.setPositionOffset pangea.object.setPositionOffsetResult pangea.object.setVelocity pangea.object.setVelocityResult pangea.object.setRotation pangea.object.setRotationResult pangea.object.setScale
pangea.object.setScaleResult pangea.object.setAnimation pangea.object.setAnimationResult pangea.object.setCollisionEnabled pangea.object.setCollisionEnabledResult pangea.object.setActive pangea.object.setActiveResult pangea.object.delete
pangea.object.deleteResult

API reference:
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
pangea.player.get(playerNum: number): PangeaPlayerSnapshot|nil — Returns a normalized read-only player snapshot.
pangea.player.raceResults(): PangeaRaceResult[]|nil — Returns the native read-only race result table, or nil when the selected game does not expose validated race state.
pangea.player.objectiveResults(): PangeaObjectiveResult[]|nil — Returns the bounded read-only objective result table observed from native objective completion events, or nil when no objective has completed.
pangea.player.setHealth(playerNum: number, health: number): boolean — Sets a player's normalized health when the native adapter exposes a safe health mutation boundary.
pangea.player.setHealthResult(playerNum: number, health: number): PlayerCommandResult — Sets normalized player health and returns structured status and diagnostics.
pangea.player.heal(playerNum: number, amount: number): boolean — Adds normalized health to a player, clamped to full health, when the native adapter exposes safe health read and mutation boundaries.
pangea.player.healResult(playerNum: number, amount: number): PlayerCommandResult — Heals a player and returns structured status and diagnostics.
pangea.player.setInvulnerable(playerNum: number, durationSeconds: number): boolean — Sets a player's native invulnerability timer in seconds; zero disables it.
pangea.player.setInvulnerableResult(playerNum: number, durationSeconds: number): PlayerCommandResult — Sets player invulnerability and returns structured status and diagnostics.
pangea.player.setPosition(playerNum: number, position: vector3): boolean — Teleports a player when the native adapter exposes a safe position mutation boundary.
pangea.player.setPositionResult(playerNum: number, position: vector3): PlayerCommandResult — Teleports a player and returns structured status and diagnostics.
pangea.player.setVelocity(playerNum: number, velocity: vector3): boolean — Sets a player's velocity when the native adapter exposes a safe velocity mutation boundary.
pangea.player.setVelocityResult(playerNum: number, velocity: vector3): PlayerCommandResult — Sets a player's velocity and returns structured status and diagnostics.
pangea.spawn.native(id: string, position: vector3, options: stringUnion?): ObjectHandle|nil — Spawns a native object.
pangea.spawn.nativeResult(id: unknown, position: vector3, options: table?): NativeSpawnResult — Spawns a native object and returns structured status and diagnostics.
pangea.spawn.scripted(id: string, position: vector3, options: stringUnion?): ObjectHandle|nil — Spawns a custom scripted object.
pangea.object.position(handle: objectHandle): Vector3|nil — Gets the position of an object.
pangea.object.source(handle: objectHandle): ObjectSource|nil — Gets the validated terrain, spline, or map source record for a replacement object.
pangea.object.all(): ObjectHandle[] — Returns all currently registered object handles.
pangea.object.findByTag(tag: string): ObjectHandle[] — Returns registered object handles carrying a tag.
pangea.object.nearest(origin: vector3, tag: string?): ObjectHandle|nil — Returns the nearest readable registered object, optionally filtered by tag.
pangea.object.exists(handle: objectHandle): boolean — Checks whether a generation-checked object handle is live.
pangea.object.tags(handle: objectHandle): string[] — Returns the tags assigned to an object.
pangea.object.hasTag(handle: objectHandle, tag: string): boolean — Checks whether an object has a tag.
pangea.object.state(handle: objectHandle): table|nil — Returns mutable script-owned state scoped to an object generation.
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
ItemSpawnResult PickupResult WeaponHitResult TriggerResult DamageResult PangeaCapabilities PangeaDiagnostics PangeaPlayerSnapshot
PangeaRaceResult PangeaObjectiveResult PlayerCommandResult NativeSpawnResult ObjectCommandResult

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

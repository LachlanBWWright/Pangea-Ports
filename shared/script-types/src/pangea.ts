export type GameId =
  | "BillyFrontier-Android"
  | "Bugdom-android"
  | "Bugdom2-Android"
  | "CroMagRally-Android"
  | "MightyMike-Android"
  | "Nanosaur-android"
  | "Nanosaur2-Android"
  | "OttoMatic-Android";

export interface Vector2 {
  readonly x: number;
  readonly y: number;
}

export interface Vector3 {
  readonly x: number;
  readonly y: number;
  readonly z: number;
}

export interface ObjectHandle {
  readonly id: number;
  readonly generation: number;
}

export interface ObjectSource {
  readonly kind: "terrain" | "spline" | "map" | "none";
  readonly itemIndex: number;
  readonly nativeType: number;
  readonly splineNum: number;
  readonly x: number;
  readonly y: number;
  readonly z: number;
  readonly placement: number;
}

export interface GameContext {
  readonly gameId: GameId;
  readonly gameName: string;
  readonly mode?: string;
  readonly networked?: boolean;
  readonly trackName?: string;
  readonly sceneName?: string;
  readonly areaName?: string;
  readonly playerMode?: string;
  readonly modePhase?: number;
  readonly modeWave?: number;
  readonly modeTimer?: number;
}

export interface LevelContext extends GameContext {
  readonly levelNum: number;
  readonly levelName?: string;
}

export interface FrameContext extends LevelContext {
  readonly frameNum: number;
  readonly deltaSeconds: number;
  readonly levelTimeSeconds: number;
}

export interface ObjectFrameContext extends FrameContext {
  readonly object: ObjectHandle;
  readonly objectType: string;
  readonly position: Vector3;
  readonly tags: readonly string[];
  readonly event: ObjectEvent;
}

export interface ObjectEventContext extends FrameContext {
  readonly object: ObjectHandle;
  readonly position: Vector3;
  readonly velocity?: Vector3;
  readonly collisionEnabled?: boolean;
  readonly rotation?: Vector3;
  readonly objectType: string;
  readonly tags: readonly string[];
  readonly other?: ObjectHandle;
  readonly sideBits?: number;
  readonly eventValue?: number;
}

export type ObjectEvent =
  | "spawn"
  | "update"
  | "triggerEnter"
  | "animationEvent"
  | "animationComplete"
  | "activate"
  | "deactivate"
  | "streamIn"
  | "streamOut"
  | "checkpointReset"
  | "destroy";

export interface ObjectFrameResult {
  readonly positionOffset?: Vector3;
}

export interface PickupContext extends LevelContext {
  readonly playerNum: number;
  readonly pickupId?: string;
  readonly pickupType: number;
  readonly pickupVariant: number;
  readonly amount: number;
  readonly pickup: ObjectHandle;
  readonly player?: ObjectHandle;
  readonly position: Vector3;
}

export interface PickupResult {
  readonly handled?: boolean;
  readonly consumePickup?: boolean;
  readonly scoreDelta?: number;
  readonly healthDelta?: number;
}

export interface WeaponHitContext extends LevelContext {
  readonly playerNum: number;
  readonly weaponId?: string;
  readonly weaponType: number;
  readonly damage: number;
  readonly weapon?: ObjectHandle;
  readonly target?: ObjectHandle;
  readonly position: Vector3;
  readonly targetType: number;
  readonly targetFlags: number;
}

export interface WeaponHitResult {
  readonly handled?: boolean;
  readonly applyDamage?: boolean;
  readonly damage?: number;
  readonly destroyTarget?: boolean;
  readonly scoreDelta?: number;
}

export interface TriggerContext extends LevelContext {
  readonly playerNum: number;
  readonly triggerId?: string;
  readonly triggerType: number;
  readonly self: ObjectHandle;
  readonly other?: ObjectHandle;
  readonly position: Vector3;
  readonly sideBits: number;
  readonly otherType: number;
  readonly otherFlags: number;
}

export interface TriggerResult {
  readonly handled?: boolean;
  readonly solid?: boolean;
  readonly deleteSelf?: boolean;
  readonly deleteOther?: boolean;
  readonly damagePlayer?: number;
  readonly healthDelta?: number;
  readonly scoreDelta?: number;
}

export interface DamageContext extends LevelContext {
  readonly playerNum: number;
  readonly cause: number;
  readonly damage: number;
  readonly source?: ObjectHandle;
  readonly target: ObjectHandle;
  readonly position: Vector3;
}

export interface DamageResult {
  readonly handled?: boolean;
  readonly applyDamage?: boolean;
  readonly damage?: number;
}

export interface PlayerEventContext extends LevelContext {
  readonly playerNum: number;
  readonly player: ObjectHandle;
  readonly position?: Vector3;
  readonly velocity?: Vector3;
  readonly eventValue?: number;
}

export type ObjectiveOutcome = 0 | 1 | 2;

export interface ObjectiveEventContext extends PlayerEventContext {
  readonly eventValue: ObjectiveOutcome;
}

export interface TerrainItemContext extends LevelContext {
  readonly itemType: number;
  readonly remappedItemType: number;
  readonly position: Vector3;
  readonly flags: number;
  readonly params: readonly number[];
}

export interface SplineItemContext extends LevelContext {
  readonly itemType: number;
  readonly splineNum: number;
  readonly placement: number;
  readonly params: readonly number[];
}

export interface MikeMapItemContext extends GameContext {
  readonly scene: number;
  readonly area: number;
  readonly itemType: number;
  readonly position: Vector2;
  readonly params: readonly number[];
}

export type ItemSpawnResult =
  | { readonly handled: true; readonly markInUse?: boolean }
  | { readonly handled: false };

export interface ScriptedObjectSelf {
  readonly handle: ObjectHandle;
  readonly state: Record<string, unknown>;
}

export interface ScriptedObjectDefinition {
  readonly onSpawn?: (self: ScriptedObjectSelf, ctx: LevelContext) => void;
  readonly onUpdate?: (self: ScriptedObjectSelf, ctx: FrameContext) => void;
  readonly onTriggerEnter?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onTriggerStay?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onTriggerExit?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onCollision?: (self: ScriptedObjectSelf, other: ObjectHandle, ctx: FrameContext) => void;
  readonly onAnimationEvent?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onAnimationComplete?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onActivate?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onDeactivate?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onStreamIn?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onStreamOut?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onCheckpointReset?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onDestroy?: (self: ScriptedObjectSelf, ctx: ObjectEventContext) => void;
  readonly onDamage?: (
    self: ScriptedObjectSelf,
    amount: number,
    ctx: FrameContext,
  ) => number;
  readonly onDelete?: (self: ScriptedObjectSelf, ctx: LevelContext) => void;
}

export interface NativeSpawnOptions {
  readonly amount?: number;
  readonly param0?: number;
  readonly param1?: number;
  readonly param2?: number;
  readonly param3?: number;
  readonly subtype?: number;
}

export interface NativeSpawnResult {
  readonly ok: boolean;
  readonly code: number;
  readonly message: string;
  readonly reason: "ok" | "not-enabled" | "file-not-found" | "parse-error" | "runtime-error" | "bad-argument" | "budget-exceeded" | "incompatible-item" | "config-error" | "unknown";
  readonly primary?: ObjectHandle;
}

export interface ObjectCommandResult {
  readonly ok: boolean;
  readonly code: number;
  readonly reason: NativeSpawnResult["reason"];
  readonly message: string;
  readonly primary?: ObjectHandle;
}

export interface PlayerCommandResult {
  readonly ok: boolean;
  readonly code: number;
  readonly reason: NativeSpawnResult["reason"];
  readonly message: string;
}

export interface ScriptedSpawnOptions {
  readonly scale?: number;
  readonly animation?: string | number;
  readonly animationSpeed?: number;
  readonly blendSeconds?: number;
}

export interface PangeaCapabilities {
  readonly contractVersion: number;
  readonly apiVersion: number;
  readonly runtimeFingerprint: number;
  readonly objectPosition: boolean;
  readonly objectMutation: boolean;
  readonly spawnNative: boolean;
  readonly spawnScripted: boolean;
  readonly objectQueries: boolean;
  readonly timers: boolean;
  readonly tasks: boolean;
  readonly events: boolean;
  readonly persistence: boolean;
  readonly memoryLimitBytes: number;
  readonly loadInstructionBudget: number;
  readonly eventInstructionBudget: number;
  readonly frameInstructionBudget: number;
  readonly timerLimit: number;
  readonly taskLimit: number;
  readonly subscriptionLimit: number;
  readonly levelSettings: boolean;
  readonly playerLookup: boolean;
  readonly terrainItems: boolean;
  readonly splineItems: boolean;
  readonly mapItems: boolean;
  readonly pickupScoreEffects: boolean;
  readonly objectCollision: boolean;
  readonly playerCommands: boolean;
  readonly playerInvulnerability: boolean;
  readonly weaponScoreEffects: boolean;
  readonly playerForm: boolean;
  readonly raceMetadata: boolean;
  readonly objectiveMetadata: boolean;
  readonly checkpointEvents: boolean;
  readonly levelMetadata: boolean;
}

export interface PlayerWeapon {
  readonly type: number;
  readonly quantity: number;
}

export interface EggProgress {
  readonly recovered: number;
  readonly required: number;
}

export interface PlayerSnapshot {
  readonly playerNum: number;
  readonly position: Vector3;
  readonly velocity?: Vector3;
  readonly rotation?: Vector3;
  readonly aim?: Vector3;
  readonly health?: number;
  readonly fuel?: number;
  readonly score?: number;
  readonly coinCount?: number;
  readonly pesoCount?: number;
  readonly lives?: number;
  readonly activeWeapon?: number;
  readonly weapons?: readonly PlayerWeapon[];
  readonly keys?: readonly number[];
  readonly greenCloverCount?: number;
  readonly blueCloverCount?: number;
  readonly goldCloverCount?: number;
  readonly tokenCount?: number;
  readonly shieldActive?: boolean;
  readonly form?: "bug" | "ball";
  readonly miceRescued?: number;
  readonly miceTotal?: number;
  readonly drowningMiceRescued?: number;
  readonly drowningMiceRequired?: number;
  readonly childObjectCount?: number;
  readonly eggs?: readonly EggProgress[];
  readonly lapNum?: number;
  readonly checkpointNum?: number;
  readonly placement?: number;
  readonly raceComplete?: boolean;
  readonly vehicleType?: number;
  readonly vehicleMaxSpeed?: number;
  readonly vehicleAcceleration?: number;
  readonly vehicleTraction?: number;
  readonly vehicleSuspension?: number;
  readonly team?: number;
  readonly carryingFlag?: boolean;
  readonly captureScore?: number;
  readonly sceneNum?: number;
  readonly areaNum?: number;
  readonly areaComplete?: boolean;
  readonly camera?: Vector3;
}

export interface RaceResult {
  readonly playerNum: number;
  readonly lapNum: number;
  readonly checkpointNum: number;
  readonly placement: number;
  readonly raceComplete: boolean;
}

export interface ObjectiveResult {
  readonly levelNum: number;
  readonly playerNum: number;
  readonly outcome: 0 | 1 | 2;
}

export interface CheckpointResult {
  readonly levelNum: number;
  readonly playerNum: number;
  readonly checkpoint: number;
}

export interface PangeaDiagnostics {
  readonly memoryUsedBytes: number;
  readonly memoryLimitBytes: number;
  readonly activeTimers: number;
  readonly activeTasks: number;
  readonly activeSubscriptions: number;
  readonly frameNum: number;
  readonly commandCount: number;
  readonly commandHash: number;
  readonly commandTraceOverflow: boolean;
  readonly commandTrace: readonly PangeaCommandTraceEntry[];
  readonly lifecycleEventCount: number;
  readonly lifecycleTraceOverflow: boolean;
  readonly lifecycleTrace: readonly PangeaLifecycleTraceEntry[];
  readonly persistentBytes: number;
  readonly persistentEntries: number;
}

export interface PangeaCommandTraceEntry {
  readonly id: string;
  readonly objectId: number;
  readonly generation: number;
  readonly status: number;
}

export interface PangeaLifecycleTraceEntry {
  readonly id: string;
  readonly objectId: number;
  readonly generation: number;
  readonly status: number;
}

export interface PangeaApi {
  readonly api: {
    readonly version: 1;
    readonly minimumVersion: 1;
    requireVersion(minimum: number, maximum?: number): true;
    capabilities(): PangeaCapabilities;
    diagnostics(): PangeaDiagnostics;
  };
  readonly game: {
    readonly id: GameId;
    readonly name: string;
    readonly supportedHooks: readonly string[];
    readonly tags: readonly string[];
  };
  readonly log: {
    info(message: string): void;
    warn(message: string): void;
    error(message: string): void;
  };
  readonly object: {
    all(): readonly ObjectHandle[];
    findByTag(tag: string): readonly ObjectHandle[];
    nearest(origin: Vector3, tag?: string): ObjectHandle | undefined;
    exists(handle: ObjectHandle): boolean;
    position(handle: ObjectHandle): Vector3 | undefined;
    velocity(handle: ObjectHandle): Vector3 | undefined;
    rotation(handle: ObjectHandle): Vector3 | undefined;
    scale(handle: ObjectHandle): number | undefined;
    animation(handle: ObjectHandle): number | undefined;
    active(handle: ObjectHandle): boolean | undefined;
    collisionEnabled(handle: ObjectHandle): boolean | undefined;
    source(handle: ObjectHandle): ObjectSource | undefined;
    setPosition(handle: ObjectHandle, position: Vector3): boolean;
    setPositionResult(handle: ObjectHandle, position: Vector3): ObjectCommandResult;
    setPositionOffset(handle: ObjectHandle, offset: Vector3): boolean;
    setPositionOffsetResult(handle: ObjectHandle, offset: Vector3): ObjectCommandResult;
    setVelocity(handle: ObjectHandle, velocity: Vector3): boolean;
    setVelocityResult(handle: ObjectHandle, velocity: Vector3): ObjectCommandResult;
    setRotation(handle: ObjectHandle, rotation: Vector3): boolean;
    setRotationResult(handle: ObjectHandle, rotation: Vector3): ObjectCommandResult;
    setScale(handle: ObjectHandle, scale: number): boolean;
    setScaleResult(handle: ObjectHandle, scale: number): ObjectCommandResult;
    setAnimation(
      handle: ObjectHandle,
      animation: string | number,
      speed?: number,
      blendSeconds?: number,
    ): boolean;
    setAnimationResult(
      handle: ObjectHandle,
      animation: string | number,
      speed?: number,
      blendSeconds?: number,
    ): ObjectCommandResult;
    setCollisionEnabled(handle: ObjectHandle, enabled: boolean): boolean;
    setCollisionEnabledResult(handle: ObjectHandle, enabled: boolean): ObjectCommandResult;
    setActive(handle: ObjectHandle, active: boolean): boolean;
    setActiveResult(handle: ObjectHandle, active: boolean): ObjectCommandResult;
    captureCheckpoint(handle: ObjectHandle): boolean;
    restoreCheckpoint(handle: ObjectHandle): boolean;
    tags(handle: ObjectHandle): readonly string[];
    hasTag(handle: ObjectHandle, tag: string): boolean;
    state(handle: ObjectHandle): Record<string, unknown> | undefined;
    delete(handle: ObjectHandle): boolean;
    deleteResult(handle: ObjectHandle): ObjectCommandResult;
  };
  readonly spawn: {
    native(
      id: string,
      position: Vector3,
      options?: NativeSpawnOptions,
    ): ObjectHandle | undefined;
    nativeResult(
      id: string | number,
      position: Vector3,
      options?: NativeSpawnOptions,
    ): NativeSpawnResult;
    scripted(
      id: string,
      position: Vector3,
      options?: ScriptedSpawnOptions,
    ): ObjectHandle | undefined;
  };
  readonly level: {
    current(): number;
    setting(key: string): string | number | boolean | undefined;
  };
  readonly time: {
    frame(): number;
    delta(): number;
    level(): number;
    after(delaySeconds: number, callback: () => void): number;
    every(intervalSeconds: number, callback: () => void): number;
    cancel(timerId: number): boolean;
    isActive(timerId: number): boolean;
  };
  readonly task: {
    start(callback: () => void): number;
    wait(delaySeconds: number): void;
    cancel(taskId: number): boolean;
    isActive(taskId: number): boolean;
  };
  readonly events: {
    on(eventName: string, callback: (payload: unknown) => void): number;
    once(eventName: string, callback: (payload: unknown) => void): number;
    off(subscriptionId: number): boolean;
    emit(eventName: string, payload?: unknown): number;
  };
  readonly random: {
    number(): number;
    integer(minimum: number, maximum: number): number;
    seed(seed: number): void;
  };
  readonly player: {
    count(): number;
    get(playerNum: number): PlayerSnapshot | undefined;
    raceResults(): readonly RaceResult[] | undefined;
    objectiveResults(): readonly ObjectiveResult[] | undefined;
    checkpointResults(): readonly CheckpointResult[] | undefined;
    setHealth(playerNum: number, health: number): boolean;
    setHealthResult(playerNum: number, health: number): PlayerCommandResult;
    setLives(playerNum: number, lives: number): boolean;
    setLivesResult(playerNum: number, lives: number): PlayerCommandResult;
    setScore(playerNum: number, score: number): boolean;
    setScoreResult(playerNum: number, score: number): PlayerCommandResult;
    setWeaponQuantity(playerNum: number, weaponType: number, quantity: number): boolean;
    setWeaponQuantityResult(playerNum: number, weaponType: number, quantity: number): PlayerCommandResult;
    setKey(playerNum: number, keyId: number, enabled: boolean): boolean;
    setKeyResult(playerNum: number, keyId: number, enabled: boolean): PlayerCommandResult;
    setCloverCount(playerNum: number, color: 0 | 1 | 2, count: number): boolean;
    setCloverCountResult(playerNum: number, color: 0 | 1 | 2, count: number): PlayerCommandResult;
    setShieldActive(playerNum: number, active: boolean): boolean;
    setShieldActiveResult(playerNum: number, active: boolean): PlayerCommandResult;
    heal(playerNum: number, amount: number): boolean;
    healResult(playerNum: number, amount: number): PlayerCommandResult;
    setInvulnerable(playerNum: number, durationSeconds: number): boolean;
    setInvulnerableResult(playerNum: number, durationSeconds: number): PlayerCommandResult;
    setPosition(playerNum: number, position: Vector3): boolean;
    setPositionResult(playerNum: number, position: Vector3): PlayerCommandResult;
    setVelocity(playerNum: number, velocity: Vector3): boolean;
    setVelocityResult(playerNum: number, velocity: Vector3): PlayerCommandResult;
    setForm(playerNum: number, form: "bug" | "ball"): boolean;
  };
  readonly persistence: {
    get(key: string, version: number): string | number | boolean | undefined;
    set(key: string, version: number, value: string | number | boolean): boolean;
    delete(key: string): boolean;
  };
}

export declare const pangea: PangeaApi;

export interface TerrainItemDefinition {
  readonly id: string;
  readonly nativeType: number;
  readonly onSpawn: (item: TerrainItemContext) => ItemSpawnResult | void;
}

export function defineTerrainItem<TDefinition extends TerrainItemDefinition>(
  definition: TDefinition,
): TDefinition {
  return definition;
}

export function defineScriptedObject(
  definition: ScriptedObjectDefinition,
): ScriptedObjectDefinition {
  return definition;
}

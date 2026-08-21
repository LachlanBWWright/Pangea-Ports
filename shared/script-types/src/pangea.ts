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

export interface GameContext {
  readonly gameId: GameId;
  readonly gameName: string;
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
  readonly onCollision?: (
    self: ScriptedObjectSelf,
    other: ObjectHandle,
    ctx: FrameContext,
  ) => void;
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

export interface ScriptedSpawnOptions {
  readonly scale?: number;
  readonly animation?: string | number;
  readonly animationSpeed?: number;
  readonly blendSeconds?: number;
}

export interface PangeaCapabilities {
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
  readonly persistentBytes: number;
  readonly persistentEntries: number;
}

export interface PangeaCommandTraceEntry {
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
    setPosition(handle: ObjectHandle, position: Vector3): boolean;
    setPositionResult(handle: ObjectHandle, position: Vector3): ObjectCommandResult;
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
    get(playerNum: number): Readonly<{
      playerNum: number;
      position: Vector3;
      health?: number;
    }> | undefined;
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

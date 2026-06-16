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
  readonly position: Vector3;
  readonly tags: readonly string[];
}

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
  readonly subtype?: number;
}

export interface ScriptedSpawnOptions {
  readonly speed?: number;
  readonly amount?: number;
  readonly radius?: number;
}

export interface PangeaCapabilities {
  readonly objectPosition: boolean;
  readonly objectMutation: boolean;
  readonly spawnNative: boolean;
  readonly spawnScripted: boolean;
  readonly levelSettings: boolean;
}

export interface PangeaExperimentalApi {
  readonly level?: {
    current(): number;
  };
  readonly time?: {
    delta(): number;
  };
  readonly player?: {
    get(playerNum: number): ObjectHandle | undefined;
  };
  readonly spawn?: {
    readonly scripted?: (
      id: string,
      position: Vector3,
      options?: ScriptedSpawnOptions,
    ) => ObjectHandle | undefined;
  };
}

export interface PangeaApi {
  readonly api: {
    readonly version: 1;
    capabilities(): PangeaCapabilities;
  };
  readonly game: {
    readonly id: GameId;
    readonly name: string;
  };
  readonly log: {
    info(message: string): void;
    warn(message: string): void;
    error(message: string): void;
  };
  readonly object: {
    position(handle: ObjectHandle): Vector3 | undefined;
    setPosition(handle: ObjectHandle, position: Vector3): boolean;
    setVelocity(handle: ObjectHandle, velocity: Vector3): boolean;
    delete(handle: ObjectHandle): boolean;
  };
  readonly spawn: {
    native(
      id: string,
      position: Vector3,
      options?: NativeSpawnOptions,
    ): ObjectHandle | undefined;
  };
  readonly experimental?: PangeaExperimentalApi;
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

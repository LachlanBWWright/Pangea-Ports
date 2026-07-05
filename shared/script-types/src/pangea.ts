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

export interface ObjectInfo {
  readonly type?: number;
  readonly kind?: number;
  readonly mode?: number;
  readonly statusBits?: number;
  readonly cType?: number;
  readonly cBits?: number;
  readonly health?: number;
  readonly damage?: number;
  readonly velocity?: Vector3;
}

export interface ObjectBounds {
  readonly left: number;
  readonly right: number;
  readonly front: number;
  readonly back: number;
  readonly top: number;
  readonly bottom: number;
}

export interface PlayerInfo {
  readonly playerNum: number;
  readonly score?: number;
  readonly health?: number;
  readonly lives?: number;
  readonly ammo?: number;
  readonly fuel?: number;
  readonly shield?: number;
  readonly currency?: number;
  readonly inventoryType?: number;
  readonly inventoryQuantity?: number;
  readonly boostTimer?: number;
  readonly tractionTimer?: number;
  readonly invisibilityTimer?: number;
  readonly hazardTimer?: number;
  readonly collectibleA?: number;
  readonly collectibleB?: number;
  readonly collectibleC?: number;
  readonly collectibleD?: number;
}

export interface SoundOptions {
  readonly position?: Vector3;
  readonly volume?: number;
  readonly rate?: number;
}

export interface EffectOptions {
  readonly position?: Vector3;
  readonly velocity?: Vector3;
  readonly scale?: number;
  readonly quantity?: number;
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

export interface PickupContext extends LevelContext {
  readonly playerNum: number;
  readonly pickupId?: string;
  readonly pickupType: number;
  readonly amount: number;
  readonly pickup: ObjectHandle;
  readonly player: ObjectHandle;
  readonly position: Vector3;
}

export interface WeaponHitContext extends LevelContext {
  readonly playerNum: number;
  readonly weaponId?: string;
  readonly weaponType: number;
  readonly damage: number;
  readonly weapon: ObjectHandle;
  readonly target: ObjectHandle;
  readonly position: Vector3;
  readonly targetType: number;
  readonly targetFlags: number;
}

export interface TriggerContext extends LevelContext {
  readonly playerNum: number;
  readonly triggerId?: string;
  readonly triggerType: number;
  readonly self: ObjectHandle;
  readonly other: ObjectHandle;
  readonly position: Vector3;
  readonly sideBits: number;
  readonly otherType: number;
  readonly otherFlags: number;
}

export interface ObjectCollisionContext extends LevelContext {
  readonly playerNum: number;
  readonly collisionId?: string;
  readonly collisionType: number;
  readonly self: ObjectHandle;
  readonly other: ObjectHandle;
  readonly position: Vector3;
  readonly sideBits: number;
  readonly selfType: number;
  readonly selfFlags: number;
  readonly otherType: number;
  readonly otherFlags: number;
  readonly damage: number;
}

export interface PlayerDamageContext extends LevelContext {
  readonly playerNum: number;
  readonly damageId?: string;
  readonly damageType: number;
  readonly damage: number;
  readonly source: ObjectHandle;
  readonly player: ObjectHandle;
  readonly position: Vector3;
}

export interface ObjectDamageContext extends LevelContext {
  readonly playerNum: number;
  readonly damageId?: string;
  readonly damageType: number;
  readonly damage: number;
  readonly source: ObjectHandle;
  readonly target: ObjectHandle;
  readonly position: Vector3;
  readonly targetType: number;
  readonly targetFlags: number;
}

export interface ObjectDeleteContext extends LevelContext {
  readonly object: ObjectHandle;
  readonly position: Vector3;
  readonly tags: readonly string[];
}

export type ItemSpawnResult =
  | { readonly handled: true; readonly markInUse?: boolean }
  | { readonly handled: false };

export type PickupResult =
  | {
      readonly handled: true;
      readonly consumePickup?: boolean;
      readonly scoreDelta?: number;
      readonly healthDelta?: number;
    }
  | {
      readonly handled: false;
      readonly scoreDelta?: number;
      readonly healthDelta?: number;
    };

export type WeaponHitResult =
  | {
      readonly handled: true;
      readonly applyDamage?: boolean;
      readonly damage?: number;
      readonly destroyTarget?: boolean;
      readonly scoreDelta?: number;
    }
  | {
      readonly handled: false;
      readonly damage?: number;
      readonly scoreDelta?: number;
    };

export type TriggerResult =
  | {
      readonly handled: true;
      readonly solid?: boolean;
      readonly deleteSelf?: boolean;
      readonly deleteOther?: boolean;
      readonly damagePlayer?: number;
      readonly healthDelta?: number;
      readonly scoreDelta?: number;
    }
  | {
      readonly handled: false;
      readonly damagePlayer?: number;
      readonly healthDelta?: number;
      readonly scoreDelta?: number;
    };

export type ObjectCollisionResult =
  | {
      readonly handled: true;
      readonly suppressNative?: boolean;
      readonly deleteSelf?: boolean;
      readonly deleteOther?: boolean;
      readonly applyDamage?: boolean;
      readonly damage?: number;
      readonly scoreDelta?: number;
      readonly healthDelta?: number;
    }
  | {
      readonly handled: false;
      readonly damage?: number;
      readonly scoreDelta?: number;
      readonly healthDelta?: number;
    };

export type PlayerDamageResult =
  | {
      readonly handled: true;
      readonly applyDamage?: boolean;
      readonly damage?: number;
      readonly healthDelta?: number;
      readonly scoreDelta?: number;
    }
  | {
      readonly handled: false;
      readonly damage?: number;
      readonly healthDelta?: number;
      readonly scoreDelta?: number;
    };

export type ObjectDamageResult =
  | {
      readonly handled: true;
      readonly applyDamage?: boolean;
      readonly damage?: number;
      readonly destroyTarget?: boolean;
      readonly scoreDelta?: number;
    }
  | {
      readonly handled: false;
      readonly damage?: number;
      readonly scoreDelta?: number;
    };

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
  readonly onDelete?: (self: ScriptedObjectSelf, ctx: ObjectDeleteContext) => void;
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
  readonly objectTags: boolean;
  readonly objectDynamicTags: boolean;
  readonly objectState: boolean;
  readonly objectInfo: boolean;
  readonly objectBounds: boolean;
  readonly objectParams: boolean;
  readonly playerInfo: boolean;
  readonly effects: boolean;
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
    get(playerNum: number): PlayerInfo | undefined;
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
    exists(handle: ObjectHandle): boolean;
    position(handle: ObjectHandle): Vector3 | undefined;
    info(handle: ObjectHandle): ObjectInfo | undefined;
    bounds(handle: ObjectHandle): ObjectBounds | undefined;
    params(handle: ObjectHandle): readonly number[] | undefined;
    setPosition(handle: ObjectHandle, position: Vector3): boolean;
    setVelocity(handle: ObjectHandle, velocity: Vector3): boolean;
    setInfo(handle: ObjectHandle, info: ObjectInfo): boolean;
    setBounds(handle: ObjectHandle, bounds: ObjectBounds): boolean;
    setParams(handle: ObjectHandle, params: readonly number[]): boolean;
    tags(handle: ObjectHandle): readonly string[];
    hasTag(handle: ObjectHandle, tag: string): boolean;
    addTag(handle: ObjectHandle, tag: string): boolean;
    removeTag(handle: ObjectHandle, tag: string): boolean;
    state(handle: ObjectHandle): Record<string, unknown> | undefined;
    delete(handle: ObjectHandle): boolean;
    requestDelete(handle: ObjectHandle): boolean;
  };
  readonly player: {
    info(playerNum?: number): PlayerInfo | undefined;
    setInfo(playerNum: number, info: Omit<PlayerInfo, "playerNum">): boolean;
  };
  readonly effects: {
    playSound(soundId: number, options?: SoundOptions): boolean;
    spawn(effectId: number, options?: EffectOptions): boolean;
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

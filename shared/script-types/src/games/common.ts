import type {
  FrameContext,
  GameContext,
  ItemSpawnResult,
  LevelContext,
  MikeMapItemContext,
  ObjectCollisionContext,
  ObjectCollisionResult,
  ObjectDamageContext,
  ObjectDamageResult,
  ObjectDeleteContext,
  ObjectFrameContext,
  ObjectFrameResult,
  PlayerDamageContext,
  PlayerDamageResult,
  PickupContext,
  PickupResult,
  SplineItemContext,
  TerrainItemContext,
  TriggerContext,
  TriggerResult,
  WeaponHitContext,
  WeaponHitResult,
} from "../pangea";

export interface AreaContext extends GameContext {
  readonly area: number;
  readonly areaName?: string;
}

export interface SceneAreaContext extends GameContext {
  readonly scene: number;
  readonly area: number;
  readonly sceneName?: string;
  readonly areaName?: string;
}

export interface RaceContext extends LevelContext {
  readonly mode: "local" | "practice" | "network";
  readonly trackName?: string;
}

export type AdventureLifecycleModule<
  TLevel extends LevelContext,
  TFrame extends FrameContext,
> = Partial<{
  onLevelLoad(ctx: TLevel): void;
  onLevelStart(ctx: TLevel): void;
  onFrame(ctx: TFrame): void;
  onObjectFrame(ctx: ObjectFrameContext): ObjectFrameResult | void;
  onLevelComplete(ctx: TLevel): void;
  onLevelUnload(ctx: TLevel): void;
  onTerrainItem(ctx: TerrainItemContext): ItemSpawnResult | void;
  onSplineItem(ctx: SplineItemContext): ItemSpawnResult | void;
  onPickupCollected(ctx: PickupContext): PickupResult | void;
  onWeaponHit(ctx: WeaponHitContext): WeaponHitResult | void;
  onTriggerEnter(ctx: TriggerContext): TriggerResult | void;
  onObjectCollision(ctx: ObjectCollisionContext): ObjectCollisionResult | void;
  onPlayerDamage(ctx: PlayerDamageContext): PlayerDamageResult | void;
  onObjectDamage(ctx: ObjectDamageContext): ObjectDamageResult | void;
  onObjectDelete(ctx: ObjectDeleteContext): void;
}>;

export type MikeLifecycleModule<
  TArea extends SceneAreaContext,
> = Partial<{
  onAreaLoad(ctx: TArea): void;
  onAreaStart(ctx: TArea): void;
  onAreaFrame(ctx: FrameContext): void;
  onAreaComplete(ctx: TArea): void;
  onObjectFrame(ctx: ObjectFrameContext): ObjectFrameResult | void;
  onMapItem(ctx: MikeMapItemContext): ItemSpawnResult | void;
  onPickupCollected(ctx: PickupContext): PickupResult | void;
  onWeaponHit(ctx: WeaponHitContext): WeaponHitResult | void;
  onTriggerEnter(ctx: TriggerContext): TriggerResult | void;
  onObjectCollision(ctx: ObjectCollisionContext): ObjectCollisionResult | void;
  onPlayerDamage(ctx: PlayerDamageContext): PlayerDamageResult | void;
  onObjectDamage(ctx: ObjectDamageContext): ObjectDamageResult | void;
  onObjectDelete(ctx: ObjectDeleteContext): void;
  onAreaUnload(ctx: TArea): void;
}>;

export type RaceLifecycleModule<TRace extends RaceContext> = Partial<{
  onRaceLoad(ctx: TRace): void;
  onRaceStart(ctx: TRace): void;
  onRaceFrame(ctx: FrameContext): void;
  onRaceComplete(ctx: TRace): void;
  onRaceUnload(ctx: TRace): void;
  onObjectFrame(ctx: ObjectFrameContext): ObjectFrameResult | void;
  onPickupCollected(ctx: PickupContext): PickupResult | void;
  onWeaponHit(ctx: WeaponHitContext): WeaponHitResult | void;
  onTriggerEnter(ctx: TriggerContext): TriggerResult | void;
  onObjectCollision(ctx: ObjectCollisionContext): ObjectCollisionResult | void;
  onPlayerDamage(ctx: PlayerDamageContext): PlayerDamageResult | void;
  onObjectDamage(ctx: ObjectDamageContext): ObjectDamageResult | void;
  onObjectDelete(ctx: ObjectDeleteContext): void;
}>;

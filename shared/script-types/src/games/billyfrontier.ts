import type { AreaContext } from "./common";
import type {
  FrameContext,
  ItemSpawnResult,
  ObjectDamageContext,
  ObjectDamageResult,
  PickupContext,
  PickupResult,
  SplineItemContext,
  TerrainItemContext,
  TriggerContext,
  TriggerResult,
  WeaponHitContext,
  WeaponHitResult,
} from "../pangea";

export type BillyAreaMode = "duel" | "shootout" | "stampede" | "targetPractice";
export type BillyTriggerId =
  | "billy.trigger"
  | "billy.peso"
  | "billy.freeLifePow"
  | "billy.boost"
  | "billy.explosiveItem";

export interface BillyAreaContext extends AreaContext {
  readonly gameId: "BillyFrontier-Android";
  readonly mode: BillyAreaMode;
}

export interface BillyFrameContext extends FrameContext {
  readonly gameId: "BillyFrontier-Android";
  readonly mode: BillyAreaMode;
}

export interface BillyTerrainItemContext extends TerrainItemContext {
  readonly gameId: "BillyFrontier-Android";
  readonly mode: BillyAreaMode;
}

export interface BillySplineItemContext extends SplineItemContext {
  readonly gameId: "BillyFrontier-Android";
  readonly mode: BillyAreaMode;
}

export interface BillyTriggerContext extends TriggerContext {
  readonly gameId: "BillyFrontier-Android";
  readonly triggerId?: BillyTriggerId;
}

export type BillyFrontierLifecycleModule = Partial<{
  onAreaLoad(ctx: BillyAreaContext): void;
  onDuelStart(ctx: BillyAreaContext): void;
  onShootoutStart(ctx: BillyAreaContext): void;
  onStampedeStart(ctx: BillyAreaContext): void;
  onTargetPracticeStart(ctx: BillyAreaContext): void;
  onAreaFrame(ctx: BillyFrameContext): void;
  onAreaComplete(ctx: BillyAreaContext): void;
  onTerrainItem(ctx: BillyTerrainItemContext): ItemSpawnResult | void;
  onSplineItem(ctx: BillySplineItemContext): ItemSpawnResult | void;
  onPickupCollected(ctx: PickupContext): PickupResult | void;
  onWeaponHit(ctx: WeaponHitContext): WeaponHitResult | void;
  onObjectDamage(ctx: ObjectDamageContext): ObjectDamageResult | void;
  onTriggerEnter(ctx: BillyTriggerContext): TriggerResult | void;
}>;

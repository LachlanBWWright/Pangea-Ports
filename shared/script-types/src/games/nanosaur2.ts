import type { AdventureLifecycleModule, RaceContext, RaceLifecycleModule } from "./common";
import type { FrameContext, ItemSpawnResult, LevelContext, SplineItemContext, TerrainItemContext, TriggerContext, TriggerResult } from "../pangea";

export type Nanosaur2Mode = "adventure" | "race" | "battle" | "capture";
export type Nanosaur2TriggerId =
  | "nanosaur2.trigger"
  | "nanosaur2.powerup"
  | "nanosaur2.weaponPow"
  | "nanosaur2.healthPow"
  | "nanosaur2.fuelPow"
  | "nanosaur2.shieldPow"
  | "nanosaur2.freeLifePow"
  | "nanosaur2.egg"
  | "nanosaur2.mine"
  | "nanosaur2.electrode"
  | "nanosaur2.forestDoorKey"
  | "nanosaur2.smackable";

export interface Nanosaur2LevelContext extends LevelContext {
  readonly gameId: "Nanosaur2-Android";
  readonly mode: Nanosaur2Mode;
  readonly networked: boolean;
}

export interface Nanosaur2FrameContext extends FrameContext {
  readonly gameId: "Nanosaur2-Android";
  readonly mode: Nanosaur2Mode;
  readonly networked: boolean;
}

export interface Nanosaur2TerrainItemContext extends TerrainItemContext {
  readonly gameId: "Nanosaur2-Android";
  readonly mode: Nanosaur2Mode;
  readonly networked: boolean;
}

export interface Nanosaur2SplineItemContext extends SplineItemContext {
  readonly gameId: "Nanosaur2-Android";
  readonly mode: Nanosaur2Mode;
  readonly networked: boolean;
}

export interface Nanosaur2RaceContext extends RaceContext {
  readonly gameId: "Nanosaur2-Android";
  readonly networked: boolean;
}

export interface Nanosaur2TriggerContext extends TriggerContext {
  readonly gameId: "Nanosaur2-Android";
  readonly triggerId?: Nanosaur2TriggerId;
}

export type Nanosaur2LifecycleModule =
  AdventureLifecycleModule<Nanosaur2LevelContext, Nanosaur2FrameContext> &
  RaceLifecycleModule<Nanosaur2RaceContext> &
  Partial<{
    onTerrainItem(ctx: Nanosaur2TerrainItemContext): ItemSpawnResult | void;
    onSplineItem(ctx: Nanosaur2SplineItemContext): ItemSpawnResult | void;
    onTriggerEnter(ctx: Nanosaur2TriggerContext): TriggerResult | void;
  }>;

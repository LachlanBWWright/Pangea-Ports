import type { AdventureLifecycleModule, RaceContext, RaceLifecycleModule } from "./common";
import type { FrameContext, ItemSpawnResult, LevelContext, SplineItemContext, TerrainItemContext } from "../pangea";

export type Nanosaur2Mode = "adventure" | "race" | "battle" | "capture";

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

export type Nanosaur2LifecycleModule =
  AdventureLifecycleModule<Nanosaur2LevelContext, Nanosaur2FrameContext> &
  RaceLifecycleModule<Nanosaur2RaceContext> &
  Partial<{
    onTerrainItem(ctx: Nanosaur2TerrainItemContext): ItemSpawnResult | void;
    onSplineItem(ctx: Nanosaur2SplineItemContext): ItemSpawnResult | void;
  }>;


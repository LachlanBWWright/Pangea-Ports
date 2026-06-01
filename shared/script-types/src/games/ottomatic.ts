import type { AdventureLifecycleModule } from "./common";
import type { FrameContext, ItemSpawnResult, LevelContext, SplineItemContext, TerrainItemContext } from "../pangea";

export interface OttoMaticLevelContext extends LevelContext {
  readonly gameId: "OttoMatic-Android";
  readonly playerMode?: "robot" | "saucer";
}

export interface OttoMaticFrameContext extends FrameContext {
  readonly gameId: "OttoMatic-Android";
}

export interface OttoMaticTerrainItemContext extends TerrainItemContext {
  readonly gameId: "OttoMatic-Android";
}

export interface OttoMaticSplineItemContext extends SplineItemContext {
  readonly gameId: "OttoMatic-Android";
}

export type OttoMaticLifecycleModule =
  AdventureLifecycleModule<OttoMaticLevelContext, OttoMaticFrameContext> &
  Partial<{
    onTerrainItem(ctx: OttoMaticTerrainItemContext): ItemSpawnResult | void;
    onSplineItem(ctx: OttoMaticSplineItemContext): ItemSpawnResult | void;
  }>;


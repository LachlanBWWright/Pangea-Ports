import type { AdventureLifecycleModule } from "./common";
import type { FrameContext, ItemSpawnResult, LevelContext, SplineItemContext, TerrainItemContext } from "../pangea";

export interface BugdomLevelContext extends LevelContext {
  readonly gameId: "Bugdom-android";
}

export interface BugdomFrameContext extends FrameContext {
  readonly gameId: "Bugdom-android";
}

export interface BugdomTerrainItemContext extends TerrainItemContext {
  readonly gameId: "Bugdom-android";
}

export interface BugdomSplineItemContext extends SplineItemContext {
  readonly gameId: "Bugdom-android";
}

export type BugdomLifecycleModule =
  AdventureLifecycleModule<BugdomLevelContext, BugdomFrameContext> &
  Partial<{
    onTerrainItem(ctx: BugdomTerrainItemContext): ItemSpawnResult | void;
    onSplineItem(ctx: BugdomSplineItemContext): ItemSpawnResult | void;
  }>;


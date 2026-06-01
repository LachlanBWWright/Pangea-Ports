import type { AdventureLifecycleModule } from "./common";
import type { FrameContext, ItemSpawnResult, LevelContext, SplineItemContext, TerrainItemContext } from "../pangea";

export interface Bugdom2LevelContext extends LevelContext {
  readonly gameId: "Bugdom2-Android";
}

export interface Bugdom2FrameContext extends FrameContext {
  readonly gameId: "Bugdom2-Android";
}

export interface Bugdom2TerrainItemContext extends TerrainItemContext {
  readonly gameId: "Bugdom2-Android";
}

export interface Bugdom2SplineItemContext extends SplineItemContext {
  readonly gameId: "Bugdom2-Android";
}

export type Bugdom2LifecycleModule =
  AdventureLifecycleModule<Bugdom2LevelContext, Bugdom2FrameContext> &
  Partial<{
    onTerrainItem(ctx: Bugdom2TerrainItemContext): ItemSpawnResult | void;
    onSplineItem(ctx: Bugdom2SplineItemContext): ItemSpawnResult | void;
  }>;

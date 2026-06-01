import type { AdventureLifecycleModule } from "./common";
import type { FrameContext, ItemSpawnResult, LevelContext, TerrainItemContext } from "../pangea";

export interface NanosaurLevelContext extends LevelContext {
  readonly gameId: "Nanosaur-android";
}

export interface NanosaurFrameContext extends FrameContext {
  readonly gameId: "Nanosaur-android";
}

export interface NanosaurTerrainItemContext extends TerrainItemContext {
  readonly gameId: "Nanosaur-android";
}

export type NanosaurLifecycleModule =
  AdventureLifecycleModule<NanosaurLevelContext, NanosaurFrameContext> &
  Partial<{
    onTerrainItem(ctx: NanosaurTerrainItemContext): ItemSpawnResult | void;
  }>;


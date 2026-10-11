import type { AreaContext, GameplayLifecycleModule } from "./common";
import type { FrameContext, ItemSpawnResult, ObjectFrameContext, ObjectFrameResult, SplineItemContext, TerrainItemContext } from "../pangea";

export type BillyAreaMode = "duel" | "shootout" | "stampede" | "targetPractice";

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

export type BillyFrontierLifecycleModule = Omit<GameplayLifecycleModule & Partial<{
  onAreaLoad(ctx: BillyAreaContext): void;
  onAreaStart(ctx: BillyAreaContext): void;
  onAreaUnload(ctx: BillyAreaContext): void;
  onAreaFrame(ctx: BillyFrameContext): void;
  onObjectFrame(ctx: ObjectFrameContext): ObjectFrameResult | void;
  onAreaComplete(ctx: BillyAreaContext): void;
  onTerrainItem(ctx: BillyTerrainItemContext): ItemSpawnResult | void;
  onSplineItem(ctx: BillySplineItemContext): ItemSpawnResult | void;
}>,
    "onPlayerRespawn" | "onCheckpointReached" | "onObjectiveComplete" | "onLapComplete" | "onRaceFinish">;

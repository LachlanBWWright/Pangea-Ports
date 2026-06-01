import type { AreaContext } from "./common";
import type { FrameContext, ItemSpawnResult, SplineItemContext, TerrainItemContext } from "../pangea";

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
}>;


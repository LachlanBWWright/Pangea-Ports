import type {
  FrameContext,
  GameContext,
  ItemSpawnResult,
  LevelContext,
  MikeMapItemContext,
  SplineItemContext,
  TerrainItemContext,
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

export interface RaceConfigContext extends RaceContext {
  readonly lapCount: number;
}

export interface RacePlayer {
  readonly playerNum: number;
  readonly local: boolean;
}

export interface PowerupContext {
  readonly id: string;
  readonly itemType: number;
}

export interface RaceResults {
  readonly placements: readonly RacePlayer[];
}

export type AdventureLifecycleModule<TLevel extends LevelContext, TFrame extends FrameContext> = Partial<{
  onLevelLoad(ctx: TLevel): void;
  onLevelStart(ctx: TLevel): void;
  onFrame(ctx: TFrame): void;
  onLevelComplete(ctx: TLevel): void;
  onLevelUnload(ctx: TLevel): void;
  onTerrainItem(ctx: TerrainItemContext): ItemSpawnResult | void;
  onSplineItem(ctx: SplineItemContext): ItemSpawnResult | void;
}>;

export type MikeLifecycleModule<TScene extends SceneAreaContext, TArea extends SceneAreaContext> = Partial<{
  onSceneLoad(ctx: TScene): void;
  onAreaLoad(ctx: TArea): void;
  onAreaStart(ctx: TArea): void;
  onAreaFrame(ctx: FrameContext): void;
  onMapItem(ctx: MikeMapItemContext): ItemSpawnResult | void;
  onAreaUnload(ctx: TArea): void;
}>;

export type RaceLifecycleModule<TRace extends RaceContext> = Partial<{
  onRaceConfig(ctx: RaceConfigContext): void;
  onRaceStart(ctx: TRace): void;
  onCheckpoint(player: RacePlayer, checkpoint: number, ctx: TRace): void;
  onLapComplete(player: RacePlayer, lap: number, ctx: TRace): void;
  onPowerupCollected(player: RacePlayer, powerup: PowerupContext, ctx: TRace): void;
  onRaceFinish(results: RaceResults, ctx: TRace): void;
}>;


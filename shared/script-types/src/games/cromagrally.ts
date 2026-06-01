import type { RaceContext, RaceLifecycleModule } from "./common";
import type { ItemSpawnResult, TerrainItemContext } from "../pangea";

export interface CroMagRaceContext extends RaceContext {
  readonly gameId: "CroMagRally-Android";
  readonly networked: boolean;
}

export interface CroMagTerrainItemContext extends TerrainItemContext {
  readonly gameId: "CroMagRally-Android";
  readonly playerNum: number;
  readonly networked: boolean;
}

export type CroMagRallyLifecycleModule =
  RaceLifecycleModule<CroMagRaceContext> &
  Partial<{
    onTerrainItem(ctx: CroMagTerrainItemContext): ItemSpawnResult | void;
  }>;


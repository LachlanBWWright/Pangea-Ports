import type { MikeLifecycleModule, SceneAreaContext } from "./common";
import type { ItemSpawnResult, MikeMapItemContext } from "../pangea";

export interface MikeSceneContext extends SceneAreaContext {
  readonly gameId: "MightyMike-Android";
}

export interface MikeAreaContext extends SceneAreaContext {
  readonly gameId: "MightyMike-Android";
}

export interface MikeMapItemHookContext extends MikeMapItemContext {
  readonly gameId: "MightyMike-Android";
}

export type MightyMikeLifecycleModule =
  MikeLifecycleModule<MikeSceneContext, MikeAreaContext> &
  Partial<{
    onMapItem(ctx: MikeMapItemHookContext): ItemSpawnResult | void;
  }>;

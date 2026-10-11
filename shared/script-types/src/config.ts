import { err, ok, type Result } from "neverthrow";
import { z } from "zod";

export interface ItemOverrideV1 {
  readonly from: number;
  readonly to: number;
}

export interface AssetDependencyV1 {
  readonly kind: string;
  readonly id: string;
}

export interface LevelSettingsV1 {
  readonly assetDependencies?: readonly AssetDependencyV1[];
  readonly [key: string]: string | number | boolean | readonly AssetDependencyV1[] | undefined;
}

export interface ScriptLevelConfigV1 {
  readonly script?: string;
  readonly extraNativeItems: readonly string[];
  readonly itemOverrides: readonly ItemOverrideV1[];
  readonly customObjects?: readonly CustomObjectConfigV1[];
  readonly terrainReplacements?: readonly TerrainReplacementV1[];
  readonly mapReplacements?: readonly MapReplacementV1[];
  readonly splineReplacements?: readonly SplineReplacementV1[];
  readonly levelSettings?: LevelSettingsV1;
  readonly skipIntro?: boolean;
  readonly rules?: Readonly<Record<string, string | number | boolean>>;
}

export interface ScriptConfigV1 {
  readonly version: 1;
  readonly levels: Readonly<Record<string, ScriptLevelConfigV1>>;
}

export interface ScriptConfigParseError {
  readonly message: string;
  readonly issues: readonly string[];
}

const nativeInteger = z.number().int().min(-2147483648).max(2147483647);
const itemOverrideSchema: z.ZodType<ItemOverrideV1> = z.object({
  from: nativeInteger.nonnegative(),
  to: nativeInteger.nonnegative(),
});

const assetDependencySchema: z.ZodType<AssetDependencyV1> = z.object({
  kind: z.string().min(1).max(31),
  id: z.string().min(1).max(95),
});

const coordinate = z.number().finite().min(-3.402823466e38).max(3.402823466e38);

const levelSettingsSchema: z.ZodType<LevelSettingsV1> = z
  .object({
    assetDependencies: z.array(assetDependencySchema).max(64).optional(),
  })
  .catchall(z.union([z.string().max(255), coordinate, z.boolean()]))
  .superRefine((settings, ctx) => {
    if (Object.keys(settings).filter(key => key !== "assetDependencies").length > 64)
      ctx.addIssue({code: "custom", message: "At most 64 scalar level settings are supported"});
    for (const key of Object.keys(settings))
      if (key.length > 63) ctx.addIssue({code: "custom", message: "Setting keys must fit 63 characters", path: [key]});
  });

// Native configuration fields are retained instead of being silently stripped.
const nativeId = z.string().min(1).max(95);
function safeAssetPath(path: string | undefined, prefix: string): boolean {
  if (!path?.startsWith(prefix)) return false;
  const suffix = path.slice(prefix.length);
  return !!suffix && !suffix.includes("..") && !suffix.includes("\\")
    && suffix.split("/").every(segment => segment !== "" && segment !== ".");
}
const customObjectSchema = z.object({
  id: nativeId,
  visual: z.object({
    kind: z.enum(["nativeDisplayGroup", "customDisplayGroup", "nativeSkeleton", "customSkeleton"]).optional(),
    group: z.string().max(31).optional(),
    modelPath: z.string().max(259).optional(),
    skeletonPath: z.string().max(259).optional(),
    modelObject: nativeInteger.nonnegative().optional(),
    skeletonType: nativeInteger.optional(),
    slot: nativeInteger.optional(),
    scale: z.number().positive().max(100).optional(),
    animationSpeed: coordinate.nonnegative().optional(),
    initialAnimation: z.union([nativeInteger, z.string().max(63)]).optional(),
    animations: z.record(z.string().min(1).max(63), nativeInteger.nonnegative()).optional(),
  }).optional(),
  collision: z.object({
    preset: z.enum(["none", "solidBox", "triggerBox", "pickup", "enemy", "platform"]).optional(),
    bounds: z.object({
      width: z.number().positive().max(1000),
      height: z.number().positive().max(1000),
      depth: z.number().positive().max(1000),
    }).optional(),
  }).optional(),
});
const validatedCustomObjectSchema = customObjectSchema.superRefine((object, ctx) => {
  const v = object.visual;
  if (v?.kind === "customDisplayGroup" && (!safeAssetPath(v.modelPath, "Data/Scripts/assets/models/") || v.modelObject === undefined))
    ctx.addIssue({code: "custom", message: "Custom display groups require a safe model path and modelObject", path: ["visual"]});
  if (v?.kind === "customSkeleton" && (!safeAssetPath(v.modelPath, "Data/Scripts/assets/skeletons/") || !safeAssetPath(v.skeletonPath, "Data/Scripts/assets/skeletons/")))
    ctx.addIssue({code: "custom", message: "Custom skeletons require safe model and skeleton paths", path: ["visual"]});
  if (v?.animations && Object.keys(v.animations).length > 16)
    ctx.addIssue({code: "custom", message: "At most 16 animations are supported", path: ["visual", "animations"]});
});
const replacementFields = {
  customObjectId: nativeId,
  itemIndex: nativeInteger.nonnegative(),
  nativeType: nativeInteger.nonnegative(),
  strict: z.boolean().optional(),
};
const terrainReplacementSchema = z.object({...replacementFields, x: coordinate.optional(), z: coordinate.optional()});
const mapReplacementSchema = z.object({...replacementFields, x: coordinate.optional(), y: coordinate.optional()});
const splineReplacementSchema = z.object({...replacementFields, splineNum: nativeInteger.nonnegative(), placement: coordinate.optional()});
export type CustomObjectConfigV1 = z.infer<typeof customObjectSchema>;
export type TerrainReplacementV1 = z.infer<typeof terrainReplacementSchema>;
export type MapReplacementV1 = z.infer<typeof mapReplacementSchema>;
export type SplineReplacementV1 = z.infer<typeof splineReplacementSchema>;

const scriptLevelConfigSchema: z.ZodType<ScriptLevelConfigV1> = z.object({
  script: z.string().min(1).max(259).optional(),
  customObjects: z.array(validatedCustomObjectSchema).max(64).optional(),
  terrainReplacements: z.array(terrainReplacementSchema).max(128).optional(),
  mapReplacements: z.array(mapReplacementSchema).max(128).optional(),
  splineReplacements: z.array(splineReplacementSchema).max(128).optional(),
  extraNativeItems: z.array(z.string().min(1)).default([]),
  itemOverrides: z.array(itemOverrideSchema).max(64).default([]),
  levelSettings: levelSettingsSchema.optional(),
  skipIntro: z.boolean().optional(),
  rules: z.record(z.string(), z.union([z.string(), z.number(), z.boolean()])).optional(),
}).superRefine((level, ctx) => {
  const ids = new Set<string>();
  for (const [index, object] of (level.customObjects ?? []).entries()) {
    if (ids.has(object.id)) ctx.addIssue({code: "custom", message: "Duplicate custom object id", path: ["customObjects", index, "id"]});
    ids.add(object.id);
  }
  for (const field of ["terrainReplacements", "mapReplacements", "splineReplacements"] as const)
    for (const [index, replacement] of (level[field] ?? []).entries())
      if (!ids.has(replacement.customObjectId)) ctx.addIssue({code: "custom", message: "Replacement references unknown custom object", path: [field, index, "customObjectId"]});
});

export const scriptConfigV1Schema: z.ZodType<ScriptConfigV1> = z.object({
  version: z.literal(1),
  levels: z.record(z.string(), scriptLevelConfigSchema),
});

export function parseScriptConfigV1(value: unknown): Result<ScriptConfigV1, ScriptConfigParseError> {
  const result = scriptConfigV1Schema.safeParse(value);
  if (result.success) {
    return ok(result.data);
  }

  return err({
    message: "Invalid Pangea script config v1",
    issues: result.error.issues.map((issue) => issue.message),
  });
}

import { err, ok, type Result } from "neverthrow";
import { z } from "zod";

export interface ItemOverrideV1 {
  readonly from: number;
  readonly to: number;
}

export interface ScriptLevelConfigV1 {
  readonly script?: string;
  readonly extraNativeItems: readonly string[];
  readonly itemOverrides: readonly ItemOverrideV1[];
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

const itemOverrideSchema: z.ZodType<ItemOverrideV1> = z.object({
  from: z.number().int().nonnegative(),
  to: z.number().int().nonnegative(),
});

const scriptLevelConfigSchema: z.ZodType<ScriptLevelConfigV1> = z.object({
  script: z.string().min(1).optional(),
  extraNativeItems: z.array(z.string().min(1)).default([]),
  itemOverrides: z.array(itemOverrideSchema).default([]),
  skipIntro: z.boolean().optional(),
  rules: z.record(z.string(), z.union([z.string(), z.number(), z.boolean()])).optional(),
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


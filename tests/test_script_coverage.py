import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import scripts.check_script_coverage as coverage


class ScriptCoverageTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="pangea-coverage-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.files = {
            "shared/script/pangea_script_contract.h": '#define PANGEA_SCRIPT_BUGDOM_HOOK_LIST(X) \\\n X("onGameStart")\n',
            "shared/script/pangea_script_backend_lua.c": '''
                static const char* hooksArray[] = {PANGEA_SCRIPT_BUGDOM_HOOK_LIST(PANGEA_SCRIPT_HOOK_NAME)};
                if (strcmp(gameId, "Bugdom-android") == 0) { hooks = hooksArray; }
                static void install_pangea(void* backend) {
                    set_function(lua, "exists", lua_exists);
                    lua_setfield(lua, -2, "object");
                }
            ''',
            "shared/script-types/src/pangea.ts": "export interface PangeaApi { readonly object: {\n exists(): boolean;\n}; }",
            "shared/script-types/src/games/common.ts": "export type GameplayLifecycleModule = Partial<{ onGameStart(ctx: GameContext): void; }>;",
            "shared/script-types/src/games/bugdom.ts": "export type BugdomLifecycleModule = GameplayLifecycleModule;",
            "games/Bugdom-android/src/Scripting/ScriptBindings.c": '''
                #define START_HOOK PANGEA_SCRIPT_HOOK_GAME_START
                static void dispatch(int hook) { PangeaScript_CallLevelHook(hook, &context); }
                void ScriptStart(void) { dispatch(START_HOOK); }
                void DeadWrapper(void) { PangeaScript_CallDamageHook(&ctx, &result); }
            ''',
            "games/Bugdom-android/src/System/Main.c": "void GameStart(void) { ScriptStart(); }",
        }

    def report(self):
        for name, text in self.files.items():
            file = self.root / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text(text)
        with patch.dict(coverage.GAMES, {"BUGDOM": ("Bugdom-android", "bugdom")}, clear=True):
            return coverage.check(self.root)

    def test_macro_and_helper_indirection_is_covered(self):
        self.assertEqual(self.report()["errors"], [])

    def test_removing_actual_engine_call_detects_dead_advertisement(self):
        self.files["games/Bugdom-android/src/System/Main.c"] = 'void GameStart(void) { /* ScriptStart(); */ puts("ScriptStart()"); }'
        self.assertIn("Bugdom-android: advertised onGameStart has no reachable native dispatch", self.report()["errors"])

    def test_enum_removal_is_not_masked_by_logging_string(self):
        name = "games/Bugdom-android/src/Scripting/ScriptBindings.c"
        self.files[name] = self.files[name].replace("dispatch(START_HOOK)", 'puts("onGameStart")')
        self.assertIn("Bugdom-android: advertised onGameStart has no reachable native dispatch", self.report()["errors"])

    def test_dead_wrapper_is_not_counted_as_native_coverage(self):
        report = self.report()
        self.assertNotIn("onDamage", report["games"]["Bugdom-android"]["native_dispatch"])

    def test_wrong_backend_game_array_binding_fails(self):
        name = "shared/script/pangea_script_backend_lua.c"
        self.files[name] = self.files[name].replace("hooks = hooksArray", "hooks = otherGameHooks")
        self.assertIn("Bugdom-android: backend game-to-hook-array mapping does not match contract", self.report()["errors"])

    def test_removed_lua_method_fails_declared_api(self):
        name = "shared/script/pangea_script_backend_lua.c"
        self.files[name] = self.files[name].replace('set_function(lua, "exists", lua_exists);', "")
        self.assertIn("pangea.object.exists: Lua/TypeScript API mismatch", self.report()["errors"])

    def test_phantom_typescript_hook_and_method_fail(self):
        self.files["shared/script-types/src/games/common.ts"] += "\n export type PhantomLifecycleModule = Partial<{ onPhantom(ctx: GameContext): void; }>;"
        self.files["shared/script-types/src/games/bugdom.ts"] += " & PhantomLifecycleModule"
        self.files["shared/script-types/src/pangea.ts"] = self.files["shared/script-types/src/pangea.ts"].replace("exists(): boolean;", "exists(): boolean;\n ghost(): void;")
        errors = self.report()["errors"]
        self.assertIn("Bugdom-android: TS hook mismatch onPhantom (phantom)", errors)
        self.assertIn("pangea.object.ghost: Lua/TypeScript API mismatch", errors)

    def test_runtime_fixture_claim_is_distinct_from_static_coverage(self):
        self.files["games/Bugdom-android/tests/script_gameplay_coverage.json"] = json.dumps({"hooks": ["onPhantom"]})
        self.assertIn("Bugdom-android: native gameplay fixture claims unsupported onPhantom", self.report()["errors"])

    def test_runtime_manifest_follows_indirect_native_calls(self):
        self.files["games/Bugdom-android/src/System/Main.c"] += "\nvoid NativeParent(void) { NativeChild(); }\nvoid NativeChild(void) { }"
        self.files["games/Bugdom-android/tests/native_gameplay_test.c"] = "int main(void) { NativeParent(); }"
        self.files["games/Bugdom-android/tests/script_gameplay_coverage.json"] = json.dumps({"native_calls": ["NativeChild"]})
        self.assertEqual(self.report()["errors"], [])
        self.files["games/Bugdom-android/tests/native_gameplay_test.c"] = "int main(void) { }"
        self.assertTrue(any("NativeChild lacks" in error for error in self.report()["errors"]))

    def test_callback_installation_is_followed(self):
        self.files["games/Bugdom-android/src/Scripting/ScriptBindings.c"] = '''
            static void DamageCallback(void) { PangeaScript_CallDamageHook(&ctx, &result); }
            void InstallCallbacks(void) { object->callback = DamageCallback; }
        '''
        self.files["games/Bugdom-android/src/System/Main.c"] = "void GameStart(void) { InstallCallbacks(); }"
        hooks = coverage.native_hooks({k: v for k, v in self.files.items() if k.endswith(".c")})
        self.assertIn("onDamage", hooks)


if __name__ == "__main__":
    unittest.main()

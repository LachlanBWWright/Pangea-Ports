# Android Lua smoke test

The opt-in Bugdom 2 smoke build tests packaged Lua rather than treating an app
launch as success. Real native gameplay runs for 120 frames in level zero,
completes through the normal completion hook, unloads, and loads level one.
Two distinct APK asset modules write and read persistence from their real hooks.
The harness requires ordered load/start/frame/complete/unload markers for both
levels, then force-stops and relaunches the app and requires the persisted second
run. `onFrame` also verifies the native player snapshot exists.

Build the dedicated debug APK:

```sh
cd games/Bugdom2-Android/android
./gradlew -PluaSmoke=true assembleDebug
```

With an x86_64 emulator running, from the repository root:

```sh
python3 scripts/android_lua_smoke.py \
  --apk games/Bugdom2-Android/android/app/build/outputs/apk/debug/app-debug.apk \
  --clear-data --artifacts work/android-lua-smoke-results
```

The fixture installs as `io.jor.bugdom2.luasmoke`, separate from the normal game.
`--clear-data` resets that fixture app only. Release variants are disabled when
`luaSmoke` is enabled, and fixture assets only overlay debug assets. Normal
shipping builds do not auto-complete levels or include smoke scripts.

The Build Android APK workflow runs the harness with an API 29 x86_64 emulator
and uploads telemetry results and logcat. The runner reads the real scalar
persistence record with `adb exec-out run-as`; this checks the app-writable save
location independently of whether native stdout appears in logcat. Missing
modules, missing hooks, incorrect order, failed persistence, and lost restart
state all fail the test. Host harness unit tests verify record decoding and
fixture packaging; they do not count as emulator execution.

For native offscreen verification of the same gameplay transition:

```sh
cmake -S games/Bugdom2-Android -B work/lua-smoke-native \
  -DPANGEA_ENABLE_SCRIPTING=ON -DPANGEA_ANDROID_LUA_SMOKE=ON
cmake --build work/lua-smoke-native --parallel
python3 scripts/android_lua_smoke.py \
  --native-binary work/lua-smoke-native/Bugdom2 \
  --native-directory work/lua-smoke-run --prepare-native-data --clear-data \
  --artifacts work/lua-smoke-native-results
```

The native runner creates a separate data overlay and copies the executable so
its executable-relative data lookup reaches the fixture. It sets SDL offscreen
video and dummy audio drivers. Native verification covers actual level hooks,
module selection, gameplay frames, and persisted restart state; APK asset
loading and Android writable paths still require the emulator run.

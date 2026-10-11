#!/usr/bin/env python3
"""Run the complete older-port executables offscreen with a real Lua lifecycle fixture.

Assets are hardlinked into temporary directories; replaced script files are unlinked
first, so neither shipped source assets nor build assets are modified. This smoke
checks real level/frame/object dispatch and player bindings, not collision coverage.
"""
import argparse
import os
import signal
from pathlib import Path
import shutil
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor

GAMES = {"Bugdom-android": "Bugdom", "Bugdom2-Android": "Bugdom2",
         "OttoMatic-Android": "OttoMatic", "Nanosaur-android": "Nanosaur"}
FIXTURE = """local objectSeen, frameSeen = false, false
return {
 onLevelStart=function(ctx) pangea.log.info('NATIVE_SMOKE_START') end,
 onObjectFrame=function(ctx)
  if not objectSeen then
   assert(ctx.object ~= nil)
   objectSeen=true; pangea.log.info('NATIVE_SMOKE_OBJECT')
  end
 end,
 onFrame=function(ctx)
  if ctx.frameNum>=2 and not frameSeen then
   local player=pangea.player.get(0)
   assert(player and type(player.health)=='number')
   assert(pangea.player.setHealth(0, player.health))
   frameSeen=true; pangea.log.info('NATIVE_SMOKE_FRAME_PLAYER')
  end
 end,
}
"""


def validate_output(output, returncode, timed_out):
    """Reject crashes and Lua failures even when startup markers were emitted."""
    markers = ("NATIVE_SMOKE_START", "NATIVE_SMOKE_OBJECT", "NATIVE_SMOKE_FRAME_PLAYER")
    errors = ("assertion failed", "assertion failure", "[pangeascript error]", "scripting on")
    expected_exit = returncode == 0 or (timed_out and returncode in (-signal.SIGTERM, -signal.SIGKILL))
    return expected_exit and all(marker in output for marker in markers) and not any(error in output.lower() for error in errors)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-root',type=Path,required=True,help='Directory with lua-native-GAME CMake builds')
    parser.add_argument('--logs',type=Path,required=True)
    parser.add_argument('--timeout',type=float,default=35)
    parser.add_argument('--game',choices=GAMES,action='append')
    parser.add_argument('--sdl-lib-dir',type=Path)
    args=parser.parse_args();args.logs.mkdir(parents=True,exist_ok=True)
    def run(game):
        build=(args.build_root/('lua-native-'+game)).resolve();name=GAMES[game]
        with tempfile.TemporaryDirectory(prefix='native-lua-live-',dir=args.build_root) as directory:
            work=Path(directory)
            shutil.copytree(build/'Data',work/'Data',copy_function=os.link)
            shutil.copy2(build/name,work/name)
            for path in ('Data/Scripts/main.lua','Data/Scripts/dist/main.lua'):
                file=work/path;file.parent.mkdir(parents=True,exist_ok=True)
                file.unlink(missing_ok=True);file.write_text(FIXTURE)
            env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy')
            if args.sdl_lib_dir:env['LD_LIBRARY_PATH']=str(args.sdl_lib_dir.resolve())+(':'+env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
            process=subprocess.Popen([str(work/name),'--level','0'],cwd=work,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
            timed_out = False
            try:output,_=process.communicate(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                timed_out = True
                process.terminate()
                try:output,_=process.communicate(timeout=5)
                except subprocess.TimeoutExpired:process.kill();output,_=process.communicate()
            (args.logs/(game+'.log')).write_text(output)
            markers=('NATIVE_SMOKE_START','NATIVE_SMOKE_OBJECT','NATIVE_SMOKE_FRAME_PLAYER')
            passed=validate_output(output, process.returncode, timed_out)
            print(('PASS ' if passed else 'FAIL ')+game+': '+', '.join(marker for marker in markers if marker in output),flush=True)
            return passed
    with ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(run,args.game or GAMES))
    return not all(results)
if __name__=='__main__':raise SystemExit(main())

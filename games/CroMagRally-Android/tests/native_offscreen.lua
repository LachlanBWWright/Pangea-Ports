local frames = 0
return {
  onGameStart = function() pangea.log.info('NATIVE_LUA_SMOKE_START') end,
  onRaceFrame = function(ctx)
    frames = frames + 1
    assert(ctx.levelNum == 0)
    local player = assert(pangea.player.get(0))
    local object = assert(pangea.object.findByTag("player")[1])
    if frames == 1 then
      assert(pangea.player.setHealth(0, .625))
      assert(pangea.player.get(0).health == .625)
      assert(pangea.object.position(object) ~= nil)
      pangea.log.info('NATIVE_LUA_SMOKE_ADAPTER')
    end
    if frames == 120 then pangea.log.info('NATIVE_LUA_SMOKE_FRAMES') end
  end,
}

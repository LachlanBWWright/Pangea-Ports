local damage, applied, deaths, respawns, resets = 0, 0, 0, 0, 0
return {
 onDamage = function(ctx)
  damage = damage + 1
  assert(ctx.levelNum == 4 and ctx.playerNum == 0 and ctx.target ~= nil and ctx.source == nil)
  assert(ctx.position.x == 10 and ctx.position.y == 20 and ctx.position.z == 30)
  if damage == 1 then return {applyDamage = false} end
  if damage == 2 then return {damage = .25} end
 end,
 onDamageApplied = function(ctx)
  applied = applied + 1
  assert(ctx.damage == (applied == 1 and .25 or 2) and ctx.target ~= nil and ctx.source == nil)
 end,
 onDeath = function(ctx)
  deaths = deaths + 1
  assert(ctx.levelNum == 4 and ctx.playerNum == 0)
 end,
 onPlayerRespawn = function(ctx)
  respawns = respawns + 1
  assert(ctx.levelNum == 4 and ctx.playerNum == 0 and ctx.player ~= nil)
  local position = pangea.object.position(ctx.player)
  assert(position.x == 100 and position.z == 300)
 end,
 onObjectFrame = function(ctx)
  if ctx.event == "checkpointReset" then
   resets = resets + 1
   assert(ctx.object ~= nil)
  end
 end,
 onFrame = function(ctx)
  assert(damage == 3 and applied == 2 and deaths == 1 and respawns == 1 and resets == 2)
 end,
}

local damage, applied, deaths, laps, finishes, hits, checkpoints = 0, 0, 0, 0, 0, 0, 0
return {
  onWeaponHit = function(ctx) hits=hits+1;if hits==1 then return {applyDamage=false} end end,
  onDamage = function(ctx)
    assert(ctx.levelNum == 4 and ctx.playerNum == 0 and ctx.target ~= nil)
    damage = damage + 1
    if damage == 1 then return {applyDamage=false} end
    if damage == 2 then return {damage=.25} end
  end,
  onDamageApplied = function(ctx) applied = applied + 1; assert(ctx.damage == (applied == 1 and .25 or 2)) end,
  onDeath = function(ctx) deaths = deaths + 1; assert(ctx.levelNum == 4 and ctx.playerNum == 0) end,
  onCheckpointReached = function(ctx) checkpoints=checkpoints+1;assert(ctx.levelNum==4 and ctx.playerNum==0) end,
  onLapComplete = function(ctx) laps=laps+1; assert(ctx.levelNum==4 and ctx.playerNum==0) end,
  onRaceFinish = function(ctx) finishes=finishes+1;assert(ctx.levelNum==4 and ctx.playerNum==0) end,
  onRaceFrame = function(ctx) assert(damage == 3 and applied == 2 and deaths == 1 and laps==2 and finishes==1 and hits==2 and checkpoints==1) end,
}

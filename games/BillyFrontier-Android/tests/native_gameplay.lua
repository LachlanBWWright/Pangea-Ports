local deaths = 0
return {
 onDeath=function(ctx)
  deaths=deaths+1
  assert(ctx.levelNum==3 and ctx.playerNum==0 and ctx.player~=nil and ctx.eventValue==0)
  assert(pangea.object.exists(ctx.player))
  local position=pangea.object.position(ctx.player)
  assert(position.x==(deaths==1 and 10 or 40) and position.y==20 and position.z==30)
 end,
 onAreaFrame=function(ctx) assert(deaths==(ctx.frameNum==0 and 1 or 2)) end,
}

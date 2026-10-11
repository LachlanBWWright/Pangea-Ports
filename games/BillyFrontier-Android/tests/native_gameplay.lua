local deaths = 0
return {
 onDeath=function(ctx) deaths=deaths+1;assert(ctx.levelNum==3 and ctx.playerNum==0 and ctx.player~=nil) end,
 onAreaFrame=function(ctx) assert(deaths==1) end,
}

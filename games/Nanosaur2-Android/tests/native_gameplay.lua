local damage, applied, deaths, respawns=0,0,0,0
return {
 onDamage=function(ctx) damage=damage+1;assert(ctx.levelNum==4 and ctx.playerNum==0); if damage==1 then return {applyDamage=false} end;if damage==2 then return {damage=.25} end end,
 onDamageApplied=function(ctx) applied=applied+1;assert(ctx.damage==(applied==1 and .25 or 2)) end,
 onDeath=function(ctx) deaths=deaths+1;assert(ctx.levelNum==4 and ctx.playerNum==0) end,
 onPlayerRespawn=function(ctx) respawns=respawns+1;assert(ctx.levelNum==4 and ctx.playerNum==0 and ctx.player~=nil) end,
 onFrame=function(ctx) assert(damage==3 and applied==2 and deaths==1 and respawns==1) end,
}

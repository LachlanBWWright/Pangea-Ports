local damage, applied, pickups=0,0,0
return {
 onDamage=function(ctx) damage=damage+1;assert(ctx.levelNum==4 and ctx.playerNum==0); if damage==1 then return {applyDamage=false} end;return {damage=1.5} end,
 onDamageApplied=function(ctx) applied=applied+1;assert(ctx.damage==2) end,
 onPickupCollected=function(ctx) pickups=pickups+1;if pickups==1 then return {consumePickup=false} end end,
 onAreaFrame=function(ctx) assert(damage==2 and applied==1 and pickups==2) end,
}

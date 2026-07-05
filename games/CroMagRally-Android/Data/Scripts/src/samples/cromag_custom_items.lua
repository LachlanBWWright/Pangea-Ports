local module = {}
local helpers = require("pangea/helpers")

function module.onTriggerEnter(ctx)
  if ctx.triggerId ~= "cromag.token" then
    return { handled = false }
  end

  local state = helpers.state(ctx.self)
  state.touchCount = (state.touchCount or 0) + 1
  local player = pangea.player.info(ctx.playerNum)
  if player ~= nil then
    pangea.player.setInfo(ctx.playerNum, {
      inventoryType = player.inventoryType,
      inventoryQuantity = player.inventoryQuantity,
      tractionTimer = player.tractionTimer,
    })
  end

  return {
    handled = false,
  }
end

function module.onWeaponHit(ctx)
  if ctx.weaponId ~= "cromag.blast" then
    return { handled = false }
  end

  local targetState = helpers.state(ctx.target)
  targetState.scriptedBlastCount = (targetState.scriptedBlastCount or 0) + 1
  pangea.effects.spawn(0, {
    position = ctx.position,
    scale = 0.5,
  })

  return {
    handled = true,
    applyDamage = true,
    damage = ctx.damage,
  }
end

return module

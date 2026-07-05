local module = {}

local helpers = require("pangea/helpers")

local POWERUP_WEAPON_TYPE = 1

function module.onPickupCollected(ctx)
  if ctx.pickupId ~= "nanosaur.egg" then
    return { handled = false }
  end

  pangea.log.info("Collected scripted egg kind " .. tostring(ctx.pickupType))

  return {
    handled = false,
    scoreDelta = 25,
  }
end

function module.onWeaponHit(ctx)
  if ctx.weaponId ~= "nanosaur.weapon" then
    return { handled = false }
  end

  if ctx.weaponType ~= POWERUP_WEAPON_TYPE then
    return { handled = false }
  end

  pangea.effects.spawn(1, {
    position = ctx.position,
    scale = 0.75,
  })

  return {
    handled = true,
    applyDamage = true,
    damage = ctx.damage * 1.5,
    scoreDelta = 5,
  }
end

function module.onTriggerEnter(ctx)
  if ctx.triggerId ~= "nanosaur.powerup" then
    return { handled = false }
  end

  local state = helpers.state(ctx.self)
  state.touchCount = (state.touchCount or 0) + 1

  return {
    handled = false,
  }
end

return module

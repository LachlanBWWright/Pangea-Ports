local M = {}

local touchCount = {}

function M.onTriggerEnter(ctx)
  if ctx.triggerId ~= "nanosaur2.healthPow" then
    return { handled = false }
  end

  local key = tostring(ctx.self.id or "unknown")
  touchCount[key] = (touchCount[key] or 0) + 1

  return { handled = false }
end

function M.onWeaponHit(ctx)
  if ctx.weaponId ~= "nanosaur2.weaponHit" then
    return { handled = false }
  end

  local key = tostring(ctx.target.id or "unknown")
  touchCount[key] = (touchCount[key] or 0) + 1

  pangea.effects.spawn(2, {
    position = ctx.position,
    scale = 0.6,
  })

  return {
    handled = true,
    applyDamage = true,
    damage = ctx.damage,
  }
end

return M

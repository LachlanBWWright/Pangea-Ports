local helpers = {}

function helpers.hasTag(tags, expectedTag)
  for _, tag in ipairs(tags or {}) do
    if tag == expectedTag then
      return true
    end
  end

  return false
end

function helpers.objectHasTag(handle, expectedTag)
  return pangea.object.hasTag(handle, expectedTag)
end

function helpers.state(handle)
  return pangea.object.state(handle) or {}
end

function helpers.cooldown(state, key, durationSeconds, deltaSeconds)
  local remaining = state[key] or 0
  if remaining > 0 then
    state[key] = math.max(0, remaining - deltaSeconds)
    return false
  end

  state[key] = durationSeconds
  return true
end

function helpers.consumePickup(extra)
  local result = extra or {}
  result.handled = true
  result.consumePickup = true
  return result
end

return helpers

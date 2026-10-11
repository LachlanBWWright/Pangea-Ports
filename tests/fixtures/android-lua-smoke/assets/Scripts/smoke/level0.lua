-- Opt-in device fixture. Every marker is written through the real persistence API.
local level = 0
local module = {}
local run = 0
local wroteFrame = false
local function mark(hook)
    local events = pangea.persistence.get("android_smoke_events", 1) or ""
    events = events .. "run=" .. run .. " level=" .. level .. " hook=" .. hook .. "\n"
    assert(pangea.persistence.set("android_smoke_events", 1, events), "smoke telemetry write failed")
    assert(pangea.persistence.get("android_smoke_events", 1) == events, "smoke telemetry readback failed")
    pangea.log.info("PANGEA_ANDROID_LUA_SMOKE " .. hook .. " level=" .. level .. " run=" .. run)
end
function module.onLevelLoad(ctx)
    assert(ctx.levelNum == level and pangea.level.setting("smokeModule") == level, "wrong module/config selection")
    assert(pangea.api.capabilities().persistence, "persistence unavailable")
    if level == 0 then
        run = (pangea.persistence.get("android_smoke_runs", 1) or 0) + 1
        assert(pangea.persistence.set("android_smoke_runs", 1, run), "run counter write failed")
        if run == 1 then assert(pangea.persistence.set("android_smoke_events", 1, "")) end
    else
        run = assert(pangea.persistence.get("android_smoke_runs", 1), "run counter missing after transition")
        local events = pangea.persistence.get("android_smoke_events", 1) or ""
        assert(events:find("run=" .. run .. " level=0 hook=unload", 1, true), "previous level did not unload")
    end
    mark("load")
end
function module.onLevelStart(ctx)
    assert(ctx.levelNum == level)
    mark("start")
end
function module.onFrame(ctx)
    if wroteFrame then return end
    assert(ctx.levelNum == level and ctx.frameNum >= 0)
    assert(pangea.player.get(0), "native player unavailable during gameplay")
    wroteFrame = true
    mark("frame")
end
function module.onLevelComplete(ctx)
    assert(ctx.levelNum == level and wroteFrame, "completion before native gameplay frame")
    mark("complete")
end
function module.onLevelUnload(ctx)
    assert(ctx.levelNum == level)
    mark("unload")
end
return module

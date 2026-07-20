-- scripts/examples/onTurn.lua
-- Example general engine-event script. Declared in config/scripts/examples.json with
-- "implements" : "ON_PLAYER_TURN_START", it is fired server-side from
-- NewTurnProcessor::onPlayerTurnStarted via scripting::Context::callGlobal.
local M = {}
M.__index = M

-- 'params' is the JsonNode { player = <int>, day = <int> } marshalled to a Lua table.
-- Written with ':' so it receives (self, params); reads its identifiers from params.
function M:onPlayerTurnStart(params)
	print("[onTurn] player " .. tostring(params.player) .. " starting day " .. tostring(params.day))
end

return M

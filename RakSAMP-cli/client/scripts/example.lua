-- Example client script. Enable it with scripts="example" in RakSAMPClient.xml
-- or load it at runtime with !loadscr example

require("addon")

local floodText = nil

function onLoad()
	print("example.lua loaded")
	setRate(RATE_LUA, 100)

	newTask(function()
		while true do
			wait(1000)

			if floodText and isBotSpawned() then
				sendInput(floodText)
			end
		end
	end)
end

function onConnect()
	print("connected to " .. getServerName() .. " as id " .. getBotId())
end

function onDisconnect()
	print("disconnected")
end

function onRunCommand(cmd)
	if cmd:find("^!flood") then
		floodText = not floodText and cmd:match("^!flood (.+)$") or nil
		print(floodText and ("flooding with: " .. floodText) or "flooder off")

		return false
	end

	if cmd == "!where" then
		local x, y, z = getBotPosition()
		print(string.format("%.2f %.2f %.2f (%d players online)", x, y, z, getPlayerCount()))

		return false
	end
end

function onUnload()
	print("example.lua unloaded")
end

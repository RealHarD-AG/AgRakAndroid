-- addon: coroutine based tasks and extra bitStream methods.
-- Loaded with require("addon"). Installs newTask / wait / clearTasks as globals.

local addon = {}

local tasks = {}
local currentTask = nil

local Task = {}
Task.__index = Task

function Task:isAlive()
	return not self.dead
end

function Task:isHalted()
	return self.halted == true
end

function Task:halt()
	self.halted = true
end

function Task:resume()
	self.halted = false
	self.resumeAt = 0
end

function Task:kill()
	self.dead = true
end

function Task:tick()
	if self.dead or coroutine.status(self.co) == "dead" then
		self.dead = true
		return
	end

	local previous = currentTask
	currentTask = self

	local ok, result
	if self.args then
		ok, result = coroutine.resume(self.co, (table.unpack or unpack)(self.args))
		self.args = nil
	else
		ok, result = coroutine.resume(self.co)
	end

	currentTask = previous

	if not ok then
		print("[TASK] " .. tostring(result))
		self.dead = true
		return
	end

	if coroutine.status(self.co) == "dead" then
		self.dead = true
		return
	end

	self.resumeAt = getTickCount() + (tonumber(result) or 0)
end

function newTask(f, halted, ...)
	if type(f) ~= "function" then
		error("newTask expects a function", 2)
	end

	local task = setmetatable({
		co = coroutine.create(f),
		args = { ... },
		halted = false,
		resumeAt = 0,
		dead = false,
	}, Task)

	if halted == true then
		task.halted = true
	elseif type(halted) == "number" then
		task.resumeAt = getTickCount() + halted
	end

	tasks[#tasks + 1] = task

	return task
end

function clearTasks()
	for i = #tasks, 1, -1 do
		tasks[i].dead = true
		tasks[i] = nil
	end
end

-- Suspends the calling task for the given number of milliseconds.
function wait(time)
	if currentTask == nil then
		error("wait() can only be used inside a task", 2)
	end

	return coroutine.yield(tonumber(time) or 0)
end

function addon.tick()
	local now = getTickCount()
	local i = 1

	while i <= #tasks do
		local task = tasks[i]

		if not task.dead and not task.halted and now >= task.resumeAt then
			task:tick()
		end

		if task.dead then
			table.remove(tasks, i)
		else
			i = i + 1
		end
	end
end

function addon.getTasks()
	return tasks
end

registerHandler("onUpdate", addon.tick)
registerHandler("onUnload", clearTasks)

-- ---------------------------------------------------------------- bitStream extras

function bitStream:readString8()
	local length = self:readUInt8()

	if length == 0 then
		return ""
	end

	return self:readString(length) or ""
end

function bitStream:writeString8(value)
	value = tostring(value or "")

	self:writeUInt8(#value)
	self:writeString(value)
end

function bitStream:readBool8()
	return self:readUInt8() ~= 0
end

function bitStream:writeBool8(value)
	self:writeUInt8(value and 1 or 0)
end

function bitStream:writeArray(value)
	for i = 1, #value do
		self:writeUInt8(value[i])
	end
end

function bitStream:writeVector3(value)
	self:writeFloat(value.x or value[1])
	self:writeFloat(value.y or value[2])
	self:writeFloat(value.z or value[3])
end

function bitStream:readVector3()
	local x, y, z = self:readFloat(), self:readFloat(), self:readFloat()

	return { x, y, z, x = x, y = y, z = z }
end

return addon

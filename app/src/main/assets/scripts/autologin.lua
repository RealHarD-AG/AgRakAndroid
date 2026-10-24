require("addon")

-- Account credentials & settings (populated dynamically by BotService)
local PASSWORD = ""
local AUTO_LOGIN_ENABLED = true

local hasAttemptedLogin = false
local isReconnecting = false

function onLoad()
    print("[AutoLogin] Script active. Sleep Mode enabled.")
end

function onDisconnect()
    hasAttemptedLogin = false
    isReconnecting = false
    print("[AutoLogin] Disconnected from server. Session reset.")
end

function onPrintLog(text)
    if not text then return end

    local lower = text:lower()

    if lower:find("put to sleep") or lower:find("using raksamp") then
        print("[AutoLogin] >>> Account confirmed in Sleep Mode (اسلیپ فعال است).")
    end

    -- Reconnect trigger on server kick, disconnect, or notices
    if not isReconnecting then
        local myNick = (getBotNick and getBotNick()) or ""
        local isMyKick = (myNick ~= "" and lower:find(myNick:lower(), 1, true) and lower:find("kicked by admbot", 1, true))
        
        if lower:find("already logged in", 1, true) then
            isReconnecting = true
            print("[AutoLogin] Account already logged in! Waiting 30s before retry to avoid IP ban...")
            newTask(function()
                wait(30000)
                reconnect(5000)
            end)
        elseif lower:find("you are banned", 1, true) then
            isReconnecting = true
            print("[AutoLogin] Temporary IP ban detected. Waiting 60s before retry...")
            newTask(function()
                wait(60000)
                reconnect(5000)
            end)
        elseif lower:find("you have been kicked", 1, true) or isMyKick or lower:find("server closed the connection", 1, true) or lower:find("connection was closed", 1, true) then
            isReconnecting = true
            print("[AutoLogin] Server disconnect/kick detected: " .. text .. ". Waiting 10s before reconnecting...")
            newTask(function()
                wait(10000)
                reconnect(3000)
            end)
        end
    end
end

function onReceiveRPC(id, bs)
    if id == RPC_ScrDialogBox then
        local wDialogID = bs:readUInt16()
        local bDialogStyle = bs:readUInt8()
        local bTitleLength = bs:readUInt8()
        local szTitle = bs:readString(bTitleLength)

        -- Handle stats dialog auto-close
        if szTitle and (szTitle:lower():find("stats") or szTitle:lower():find("status")) then
            print("[AutoStats] Stats dialog received. Auto-closing...")
            newTask(function()
                wait(500)
                sendDialogResponse(wDialogID, 1, 0, "")
            end)
            return false
        end

        -- Handle launcher warning or notice dialog
        if wDialogID == 1 or (szTitle and szTitle:lower():find("launcher")) then
            print("[AutoLogin] Notice/Launcher dialog received. Auto-confirming...")
            newTask(function()
                wait(500)
                sendDialogResponse(wDialogID, 1, 0, "")
            end)
            return false
        end

        -- Check for login dialog (Dialog ID 2 or title containing login)
        if AUTO_LOGIN_ENABLED and not hasAttemptedLogin and (wDialogID == 2 or (szTitle and szTitle:lower():find("login"))) then
            hasAttemptedLogin = true
            print(string.format("[AutoLogin] Login dialog (ID: %d) detected. Automatically sending password...", wDialogID))
            
            newTask(function()
                wait(600)
                sendDialogResponse(wDialogID, 1, 0, PASSWORD)
                print("[AutoLogin] Password submitted successfully. Bot will remain in Sleep Mode (بدون اسپاون).")
                
                -- Wait for sleep state confirmation and update stats
                wait(4000)
                sendInput("/stats")
            end)
            return false
        end
    end
end

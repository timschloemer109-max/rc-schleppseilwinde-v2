-- Host-only JETI emulator bridge. No physical receiver or motor access.
-- Official JETI Lua API 1.5: socket p6, getInputs p43, telemetry p51, form p54.
local NAME = "Winch V3 virtual"
local INPUTS = {"P1", "SA", "SB", "SC"} -- For model/mixer tests: O1,O2,O3,O4.
local socket, peer, connected, enabled = nil, nil, false, false
local retryAt, connectAt, sampleAt, replyAt, txAt = 0, 0, 0, nil, 0
local tx, rx, link, lastTrace = "", "", "NOT STARTED", ""
local seq, button, hall, hallPending = 0, 0xf0, 0, false
local manualEnd, manualPause, activeEnd, valid = false, false, false, false
local leftWas, rightWas = false, false
local channels, status, readings = {1500,1500,1500,1500}, nil, {}
local function trace(s)
  if lastTrace ~= s then print("WINCHV3 " .. s); lastTrace = s end
end
local function close(s, now)
  if peer then pcall(function() peer:close() end) end
  peer, connected, tx, rx, replyAt = nil, false, "", "", nil
  link, retryAt = s, now + 1000
  trace("LINK " .. s)
end
local function hexText(s)
  if #s ~= 64 or s:find("[^%x]") then return nil end
  local out = {}
  for i=1,64,2 do
    local b = tonumber(s:sub(i,i+1),16)
    if not b or b < 32 or b > 126 then return nil end
    out[#out+1] = string.char(b)
  end
  return table.concat(out)
end
local function reply(line, now)
  local w = {}
  if #line > 768 then return false end
  for word in line:gmatch("%S+") do
    w[#w+1] = word
    if #w > 14 then return false end
  end
  if #w ~= 14 or w[1] ~= "R" then return false end
  local menu = hexText(w[12])
  if not menu or #w[13] < 16 or #w[13] > 128 or #w[13]%2 ~= 0 or
     w[13]:find("[^%x]") then return false end
  local n = {}
  for _,i in ipairs({2,4,6,7,8,9,10,11}) do
    n[i] = tonumber(w[i])
    -- The transmitter Lua build may use 32-bit integers. A decimal point avoids
    -- the 0xffffffff integer literal wrapping to -1 in that environment.
    if not n[i] or n[i] < 0 or n[i] >= 4294967296.0 or n[i]%1 ~= 0 then return false end
  end
  if n[4] < 900 or n[4] > 2100 or n[6] > 1 or n[8] > 1 or
     #w[3] > 20 or #w[5] > 30 then return false end
  local values, count = {}, 0
  if w[14] ~= "-" then
    for item in w[14]:gmatch("[^,]+") do
      count=count+1
      local id,raw,dec = item:match("^(%d+):([%-]?%d+):(%d+)$")
      id,raw,dec = tonumber(id),tonumber(raw),tonumber(dec)
      if count > 15 or not id or not raw or not dec or id < 1 or id > 15 or
         values[id] or math.abs(raw)>2147483647 or dec>3 then return false end
      values[id]={raw=raw,dec=dec}
    end
  end
  readings = values
  status={ms=n[2],state=w[3],esc=n[4],reason=w[5],fresh=n[6],arm=n[7],
    latched=n[8],rest=n[9],hall=n[10],errors=n[11],menu=menu}
  hall=math.max(hall,status.hall); replyAt=now
  trace(string.format("STATE %s PWM=%d reason=%s fresh=%d arm=%d end=%d",
    status.state,status.esc,status.reason,status.fresh,status.arm,status.latched))
  return true
end
local function positive(name)
  local v=system.getInputs(name)
  return type(v)=="number" and v>0.5
end
local function connect(now)
  if not peer then
    if now<retryAt then return end
    peer=socket.tcp()
    if not peer then link="SOCKET ERROR"; retryAt=now+1000; return end
    peer:settimeout(0); connectAt=now
    local ok,err=peer:connect("127.0.0.1",9876)
    if ok then connected=true
    elseif err~="timeout" and err~="Operation already in progress" and
           err~="Operation now in progress" then close("HOST OFFLINE",now); return end
  end
  if not connected then
    if peer:getpeername() then connected=true
    elseif now-connectAt>500 then close("HOST OFFLINE",now); return end
  end
  if connected and link~="CONNECTED" then
    link="CONNECTED"; replyAt=now; trace("LINK CONNECTED 127.0.0.1:9876")
  end
end
local function loop()
  if not enabled then return end
  local now=system.getTimeCounter()
  local v={system.getInputs(INPUTS[1],INPUTS[2],INPUTS[3],INPUTS[4])}
  valid=true
  for i=1,4 do
    if type(v[i])~="number" or v[i]~=v[i] or math.abs(v[i])>1.2 then valid=false
    else channels[i]=math.floor(1500+500*math.max(-1,math.min(1,v[i]))+0.5) end
  end
  activeEnd=manualEnd or positive("SD")
  local l,r=positive("SF"),positive("SG")
  if l and not leftWas then button=0x70 end
  if r and not rightWas then button=0xe0 end
  leftWas,rightWas=l,r
  if manualPause or positive("SE") then
    if peer or link~="PAUSED" then close("PAUSED",now) end
    return
  end
  connect(now)
  if not connected then return end
  -- Bounded nonblocking reads: at most 1024 bytes and 8 lines per Lua call.
  for _=1,4 do
    local data,err,partial=peer:receive(256)
    rx=rx..(data or partial or "")
    if #rx>2048 then close("RX LIMIT",now); return end
    if err=="closed" then close("CLOSED",now); return end
    if err and err~="timeout" then close("RX ERROR",now); return end
    if not data then break end
  end
  for _=1,8 do
    local e=rx:find("\n",1,true)
    if not e then break end
    local line=rx:sub(1,e-1):gsub("\r$","")
    rx=rx:sub(e+1)
    if not reply(line,now) then
      print("WINCHV3 REJECT "..line:sub(1,768))
      close("BAD REPLY",now); return
    end
  end
  if #rx>768 then close("LINE LIMIT",now); return end
  if replyAt and now-replyAt>750 then close("HOST STALE",now); return end
  if #tx>0 and now-txAt>100 then close("TX STALE",now); return end
  if #tx==0 then
    if hallPending then tx=string.format("H %d\n",hall); hallPending=false; txAt=now
    elseif valid and now-sampleAt>=30 then
      if seq>=2147483646 then close("SEQ RESET",now); seq=0; return end
      seq=seq+1
      tx=string.format("S %d %d %d %d %d %d %02x\n",seq,channels[1],channels[2],
        channels[3],channels[4],activeEnd and 1 or 0,button)
      button=0xf0; sampleAt=now; txAt=now
    end
  end
  if #tx>0 then
    local sent,err,partial=peer:send(tx)
    local last=sent or partial or 0
    if last>0 then tx=tx:sub(last+1) end
    if err and err~="timeout" then close("TX ERROR",now) end
  end
end
local function exValue(id)
  local v=readings[id]
  return v and string.format("%."..v.dec.."f",v.raw/10^v.dec) or "--"
end
local function draw(width,height,offset)
  local y=offset or 3
  local function line(s,bold)
    if y+13<=height then lcd.drawText(5,y,s,bold and FONT_BOLD or FONT_MINI) end
    y=y+17
  end
  local freshView=connected and replyAt and system.getTimeCounter()-replyAt<=750
  line("WINCH V3 - VIRTUAL / NO MOTOR",true)
  line(link.."  "..(valid and "inputs OK" or "INPUT MISSING"))
  line(string.format("%s=%d  %s=%d  End=%d",INPUTS[1],channels[1],INPUTS[2],channels[2],activeEnd and 1 or 0))
  if status then
    line(string.format("%s  PWM %d us  RC %s",status.state,status.esc,
      freshView and (status.fresh==1 and "OK" or "OLD") or "STALE"),true)
    line(string.format("%s arm=%d latched=%d",status.reason,status.arm,status.latched))
    line("JETIBOX: "..status.menu:sub(1,16))
    line("         "..status.menu:sub(17,32))
    line("EX PWM="..exValue(8).." Hall="..exValue(5).." flags="..exValue(10))
    line(string.format("Rest=%d Hall=%d bus=%d",status.rest,status.hall,status.errors))
  else
    line("Start simulator/jeti_bridge.py on PC")
    line("SA: -1 LEARN / 0 neutral / +1 RUN")
    line("SD endstop; SE pause; SF/SG menu")
  end
end
local function initForm()
  local endBox, pauseBox
  form.setButton(1,"Left",ENABLED); form.setButton(2,"Up",ENABLED)
  form.setButton(3,"Down",ENABLED); form.setButton(4,"Right",ENABLED)
  form.addRow(2); form.addLabel({label="Virtual end stop"})
  endBox=form.addCheckbox(manualEnd,function(v)
    manualEnd=not v; form.setValue(endBox,manualEnd)
  end)
  form.addRow(2); form.addLabel({label="Pause / lose RC"})
  pauseBox=form.addCheckbox(manualPause,function(v)
    manualPause=not v; form.setValue(pauseBox,manualPause)
  end)
  form.addRow(1)
  form.addLink(function()
    if hall<2147483646 then hall=hall+1; hallPending=true end
  end,{label="Inject one virtual Hall pulse"})
end
local function keyForm(key)
  if key==KEY_1 then button=0x70 elseif key==KEY_2 then button=0xd0
  elseif key==KEY_3 then button=0xb0 elseif key==KEY_4 then button=0xe0
  else return end
  form.preventDefault()
end
local function init()
  local device,emulator=system.getDeviceType()
  print(string.format("WINCHV3 INIT device=%s emulator=%s version=%s",tostring(device),
    tostring(emulator),tostring(system.getVersion())))
  print("WINCHV3 lua="..tostring(_VERSION).." integer-max="..tostring(math.maxinteger))
  if emulator~=1 then print("WINCHV3 DISABLED on physical transmitter"); return end
  local ok,module=pcall(require,"socket")
  if ok and type(module)=="table" and type(module.tcp)=="function" then
    socket,enabled=module,true; link="HOST OFFLINE"
    print("WINCHV3 socket="..tostring(socket._VERSION).." target=127.0.0.1:9876")
  else link="NO LUASOCKET"; print("WINCHV3 LuaSocket unavailable: "..tostring(module)) end
  if serial and serial.getPorts then
    local good,ports=pcall(serial.getPorts)
    if good and type(ports)=="table" then print("WINCHV3 ports="..table.concat(ports,",")) end
  end
  print("WINCHV3 inputs="..table.concat(INPUTS,",").." SD=end SE=pause SF/SG=menu")
  system.registerTelemetry(1,NAME,4,function(w,h) draw(w,h,3) end)
  system.registerForm(1,MENU_APPS,NAME,initForm,keyForm,function(w,h) draw(w,h,75) end)
end
local function destroy()
  if peer then pcall(function() peer:close() end) end
  peer,connected,enabled=nil,false,false
end
return {init=init,loop=loop,destroy=destroy,author="Seilwinde V3",version="1.0",name=NAME}

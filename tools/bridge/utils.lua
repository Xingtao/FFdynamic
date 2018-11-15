local http = require "resty.http"

-- module utils
local utils = {}

function utils.map(func, tbl)
  local newtbl = {}
  for i, v in pairs(tbl) do
    newtbl[i] = func(v)
  end
  return newtbl
end

function utils.filter(func, tbl)
  local newtbl = {}
  for i,v in pairs(tbl) do
    if func(v) then
      newtbl[i]=v
    end
  end
  return newtbl
end

-- bitwise operations
utils.OR, utils.XOR, utils.AND = 1, 3, 4
function utils.bitoper(a, b, oper)
   local r, m, s = 0, 2^52
   repeat
      s,a,b = a+b+m, a%m, b%m
      r,m = r + m*oper%(s-a-b), m/2
   until m < 1
   return r
end

-- uniform distribution between [0, 1]
-- avoid use os.clock() as seed to generate random number when select srs port
local A1, A2 = 727595, 798405  -- 5^17=D20*A1+A2
local D20, D40 = 1048576, 1099511627776  -- 2^20, 2^40
local X1, X2 = 0, 1
local function rand()
  local U = X2*A2
  local V = (X1*A2 + X2*A1) % D20
  V = (V*D20 + U) % D40
  X1 = math.floor(V/D20)
  X2 = V - X1*D20
  return V/D40
end

--
function utils.external_request(mtd, url, body_data)
  local httpc = http.new()
  httpc:set_timeout(500) -- 500ms timeout
  return httpc:request_uri(url, {method = mtd, body = body_data})
end

--
return utils

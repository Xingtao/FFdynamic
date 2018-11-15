-- Copyright (C) YuanSheng Wang (membphis), 360 Inc.

local lock = require "resty.lock"

local _M = {
    _VERSION = '0.01',
}

local running = true
local mt = { __index = _M }

function _M.new(opts)
    opts = opts or {}
    local unit_name = opts.unit_name
    local write_log = (nil == opts.write_log) and true or opts.write_log
    return setmetatable({start_time=ngx.now(), unit_name = unit_name,
                          write_log = write_log, _test_inits = opts.test_inits,
                          processing=nil, count = 0,
                          count_fail=0, count_succ=0}, mt)
end

-- 字颜色: 30--39
-- 30: 黑
-- 31: 红
-- 32: 绿
-- 33: 黄
-- 34: 蓝
-- 35: 紫
-- 36: 深绿
-- 37: 白色
function _M._log(self, color, ...)
  local logs = {...}
  local color_d = {black=30, green=32, red=31, yellow=33, blue=34, purple=35, dark_green=36, white=37}

  if color_d[color] then
    local function format_color( color )
      return "\x1b["..color.."m"
    end
    ngx.print(format_color(color_d[color])..table.concat( logs, " ")..'\x1b[m')
  else
    ngx.print(...)
  end

  ngx.flush()
end

function _M._log_standard_head( self )
  if not self.write_log then
    return
  end

    local fun_format = self.unit_name
    if nil == self.processing then
      fun_format = string.format("[%s] ", self.unit_name)
    else
      fun_format = string.format("  \\_[%s] ", self.processing)
    end

    self:_log("default", string.format("%0.3f", ngx.now()-self.start_time), " ")
    self:_log("green",  fun_format)
end

function _M.log( self, ... )
  if not self.write_log then
    return
  end

  local log = {...}
  table.insert(log, "\n")

  self:_log_standard_head()
  if self.processing then
    table.insert(log, 1, "↓")
  end
  self:_log("default", unpack(log))
end

function _M.log_finish_fail( self, ... )
  if not self.write_log then
    return
  end

  local log = {...}
  table.insert(log, "\n")

  self:_log_standard_head(self)
  self:_log("yellow", "fail", unpack(log))
end

function _M.log_finish_succ( self, ... )
  if not self.write_log then
    return
  end

  local log = {...}
  table.insert(log, "\n")

  self:_log_standard_head(self)
  self:_log("green", unpack(log))
end


function _M._init_test_units( self )
  if self._test_inits then
    return self._test_inits
  end

  local test_inits = {}
  for k,v in pairs(self) do
    if k:lower():sub(1, 4) == "test" and type(v) == "function" then
      table.insert(test_inits, k)
    end
  end

  table.sort( test_inits )
  self._test_inits = test_inits
  return self._test_inits
end

function _M.run(self, loop_count )
    if self.unit_name then
      self:log_finish_succ("unit test start")
    end

    self:_init_test_units()

    loop_count = loop_count or 1

    self.time_start = ngx.now()

    for i=1,loop_count do
      for _,k in pairs(self._test_inits) do
        if self.init then
          self:init()
        end

        self.processing = k
        local _, err = pcall(self[k], self)
        if err then
          self:log_finish_fail(err)
          self.count_fail = self.count_fail + 1
        else
          self:log_finish_succ("PASS")
          self.count_succ = self.count_succ + 1
        end
        self.processing = nil
        ngx.flush()

        if self.destroy then
          self:destroy()
        end
      end
    end

    self.time_ended = ngx.now()

    if self.unit_name then
        self:log_finish_succ("unit test complete")
    end
end


function _M.bench_once( premature, self, lock, micro_count )
    if premature then
        return
    end

    if micro_count > 0 then
        local test  = self.new({unit_name="bench_item", write_log=false, test_inits=self:_init_test_units()})
        test:run(micro_count)

        self.count_succ = self.count_succ + test.count_succ
        self.count_fail = self.count_fail + test.count_fail
        self.count      = self.count + micro_count
    end
    lock:unlock()
end

function _M.bench_back_worker( self, lock_key, micro_count )
    local lock = lock:new("cache_ngx", {timeout=4})
    local elapsed, err = lock:lock(lock_key)
    if not elapsed then
        return false, "the micro test must be comsume within 2sec --> err:" .. err
    end

    local ok, err = ngx.timer.at(0, _M.bench_once, self, lock, micro_count)
    if not ok then
        return false, "create the back workder failed:" .. err
    end

    return true
end

function _M.bench_stastic( self )
    local current_time = ngx.now() - self.start_time
    self:log("succ count:\t", self.count_succ, "\tQPS:\t", string.format("%.2f", self.count_succ/current_time))
    self:log("fail count:\t", self.count_fail, "\tQPS:\t", string.format("%.2f", self.count_fail/current_time))
    self:log_finish_succ("loop count:\t", self.count,      "\tQPS:\t", string.format("%.2f", self.count/current_time))
    ngx.flush()
end

function _M.bench_run(self, total_count, micro_count, parallels)
    running = true

    total_count = total_count or 100000
    micro_count = micro_count or 25
    parallels   = parallels   or 20

    self.start_time = ngx.now()
    local last_stastics_time = ngx.now()
    local begin_succ = true

    self:log_finish_succ("!!!BENCH TEST START!!")
    for i=1,total_count/micro_count,parallels do
        if not running then
            ngx.log(ngx.ERR, "!!!CLIENT ABORT!!!")
            break
        end

        for j=1,parallels do
            local ok, err = self:bench_back_worker(self.unit_name .. j, micro_count)
            if not ok then
                ngx.log(ngx.ERR, err)
                ngx.say(err)
                begin_succ = false
                break
            end
        end
        if not begin_succ then
            break
        end

        if ngx.now() - last_stastics_time > 2 then
            last_stastics_time = ngx.now()
            self:bench_stastic()
        end
    end

    if begin_succ then
        for j=1,parallels do
            self:bench_back_worker(self.unit_name .. j, 0)
        end

        self:bench_stastic()
    end
    self:log_finish_succ("!!!BENCH TEST ALL DONE!!!")
    ngx.log(ngx.ERR, "bench all done")
end


function _M.mock_run(self, mock_rules, test_run, ...)

    local old_tb = 1
    local old_func = 2
    local new_func = 3

    --mock
    for _, rule in ipairs(mock_rules) do
        local tmp = rule[old_tb][rule[old_func]]
        rule[old_tb][rule[old_func]] = rule[new_func]
        rule[new_func] = tmp
    end

    --exec test
    local ok, res, err = pcall(test_run, ...)

    --resume
    for _, rule in ipairs(mock_rules) do
        rule[old_tb][rule[old_func]] = rule[new_func]
    end

    if not ok then  -- pcall fail, the error msg stored in "res"
      error(res)
    end

    return res, err
end


local function my_clean(  )
    running = false
end
ngx.on_abort(my_clean)

return _M

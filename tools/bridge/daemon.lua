-- system
local os = require "os"
local posix = require "posix"
-- third and project
local resty_lock = require "resty.lock"
local inspect = require "inspect"
local config = require "config_bridge"
local utils = require "utils"

local daemon = {}
-- shm_daemon_process's port and pid is only operate in worker 0, so it is thread safe.
-- instance status (flags) also operation in task level, so use daemon_status needed in monitor_internal


-- daemon helpers
local function checkPidStatus(pid)
  local flags = utils.bitoper(posix.sys.wait.WNOHANG, posix.sys.wait.WUNTRACED, utils.OR)
  return posix.sys.wait(pid, flags)
end

local function spawnOneInstance(cmd_path, cmd_args)
  local pid = posix.fork()
  if pid == 0 then -- child process
    -- exec won't return if success
    local b_ok, err_desc = posix.exec(cmd_path, cmd_args)
    if b_ok == nil then
      ngx.log(ngx.ERR, "spawn-exec failed: " .. err_desc .. ": " .. cmd_path .. " " .. inspect(cmd_args))
    end
    return nil
  else
    return pid
  end
end

local function monitor_internal()
  local daemon_processes = ngx.shared.shm_daemon_process
  local ports = daemon_processes:get_keys(0) -- get all [port - pid] dict's keys
  for _, process_port in ipairs(ports) do
    local pid = daemon_processes:get(process_port)
    repeat
      if pid ~= nil then
        local _, status_str, exit_code = checkPidStatus(pid)
        if exit_code == nil then -- still running
          break
        end
        ngx.log(ngx.INFO, process_port .. " associated pid is not running, ",
                pid, status_str, exit_code, ". Spawan again")
      else
        ngx.log(ngx.INFO, process_port .. " associated pid is not exist, ", pid, ". Spawn again")
      end
      -- spawn again. TODO: may check port range to determine whether spawn again by monitor
      local new_pid = spawnOneInstance(config.task_cmd_path, {[0] = process_port})
      if new_pid == nil then
        ngx.log(ngx.ERR, "monitor spawn instance failed, try again later.")
      else
        local lock, err = resty_lock:new("shm_process_lock")
        if not lock then
          ngx.log(ngx.ERR, "failed to create lock: ", err)
          return ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)
        end

        -- daemon_process operation and task create/delete using 'daemon_lock'
        lock:lock("daemon_status_lock")
        -- check room_id existing
        local ial_info = ngx.shared.shm_ial_infos
        local val, _ = ial_info:get(ngx.var.room_id)
        if val ~= nil then
          ngx.log(ngx.ERR, "room already exist: ", ngx.var.room_id)
          lock:unlock()
          return ngx.exit(ngx.HTTP_BAD_REQUEST)
        end

        local daemon_info = ngx.shared.shm_daemon_process
        local daemon_ports = daemon_info:get_keys()
        daemon_processes:set(process_port, new_pid, 0, config.INSTANCE_AVAILABLE)
        ngx.log(ngx.INFO, "monitor spawn instance done, port: ", process_port, ", pid: ", new_pid)
      end
    until true -- end of repeat
  end -- end of for
end

-- daemon APIs
function daemon.init()
  local daemon_processes = ngx.shared.shm_daemon_process
  for i=1, config.task_instance_num do
    repeat
      local process_port = tostring(config.task_instance_start_port + i - 1)
      local pid = daemon_processes:get(process_port)
      if pid ~= nil then
        local thePid, status_str, exit_code = checkPidStatus(pid)
        if thePid == pid and exit_code == nil then
          ngx.log(ngx.INFO, process_port .. " associated pid is running. continue", pid)
          break
        else -- will spawn later of this port
          ngx.log(ngx.INFO, process_port .. " associated pid is not running, ",
                  status_str, exit_code, ". Spawan it again")
          daemon_processes:set(process_port, nil, 0, config.INSTANCE_UNAVAILABLE)
        end
      end

      local new_pid = spawnOneInstance(config.task_cmd_path, {[0] = process_port})
      if new_pid == nil then
        ngx.log(ngx.ERR, "init spawn instance failed, will kill already spwaned process and exit.")
        TODO: killSpawnedProcess(daemon_processes)
        os.exit(23) --SIGSTOP 23
      else
        daemon_processes:set(process_port, pid, 0, config.INSTANCE_AVAILABLE)
      end
    until true -- make continue behavior
  end
end

function daemon.do_monitor()
  local count = 0
  local check
  check = function (premature, count)
    if premature then
        return
    end
    count = count + 1
    local ok, err = pcall(monitor_internal)
    if not ok then
      ngx.log(ngx.ERR, "failed to run daemon check cycle: ", count, err)
    end
    -- keep this debug for now
    if (count % 600) == 599 then
      ngx.log(ngx.INFO, "daemon check cycle: ", count)
    end

    local ok, err = ngx.timer.at(config.daemon_check_period, check, count)
    if not ok then
      if err ~= "process exiting" then
        errlog("failed to create timer: ", err)
      end
    end
end

----
return daemon

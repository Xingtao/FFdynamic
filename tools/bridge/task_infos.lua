-- system
local os = require "os"
local posix = require "posix"
-- third and project
local inspect = require "inspect"
local config = require "config_bridge"
local utils = require "utils"

local task_infos = {}
-- 'task info' include interactive_live and dynamic_transcode tasks

-- task_infos helpers
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
  local task_infos_processes = ngx.shared.shm_task_infos_process
  local ports = task_infos_processes:get_keys(0) -- get all [port - pid] dict's keys
  for _, process_port in ipairs(ports) do
    local pid = task_infos_processes:get(process_port)
    repeat
      if pid ~= nil then
        local _, status_str, exit_code = checkPidStatus(pid)
        if exit_code == nil then -- still running
          break
        end
        ngx.log(ngx.INFO, process_port .. " associated pid is not running, ", pid, status_str, exit_code, ". Spawan again")
      else
        ngx.log(ngx.INFO, process_port .. " associated pid is not exist, ", pid, ". Spawn again")
      end
      -- spawn again. TODO: may check port range to determine whether spawn again by monitor
      local new_pid = spawnOneInstance(config.task_cmd_path, {[0] = process_port})
      if new_pid == nil then
        ngx.log(ngx.ERR, "monitor spawn instance failed, try again later.")
      else
        task_infos_processes:set(process_port, new_pid)
        ngx.log(ngx.INFO, "monitor spawn instance done, port: ", process_port, ", pid: ", new_pid)
      end
    until true -- end of repeat
  end -- end of for
end

-- task_infos APIs
function task_infos.init()
  local task_infos_processes = ngx.shared.shm_task_infos_process
  for i=1, config.task_instance_num do
    repeat
      local process_port = tostring(config.task_instance_start_port + i - 1)
      local pid = task_infos_processes:get(process_port)
      if pid ~= nil then
        local thePid, status_str, exit_code = checkPidStatus(pid)
        if thePid == pid and exit_code == nil then
          ngx.log(ngx.INFO, process_port .. " associated pid is running. continue", pid)
          break
        else -- will spawn
          ngx.log(ngx.INFO, process_port .. " associated pid is not running, ", status_str, exit_code, ". Spawan it again")
          task_infos_processes:set(process_port, nil)
        end
      end

      local new_pid = spawnOneInstance(config.task_cmd_path, {[0] = process_port})
      if new_pid == nil then
        ngx.log(ngx.ERR, "init spawn instance failed, will kill already spwaned process and exit.")
        TODO: killSpawnedProcess(task_infos_processes)
        os.exit(23) --SIGSTOP 23
      else
        task_infos_processes:set(process_port, pid)
      end
    until true -- make continue behavior
  end
end

function task_infos.do_monitor()
  local count = 0
  local check
  check = function (premature, count)
    if premature then
        return
    end
    count = count + 1
    local ok, err = pcall(monitor_internal)
    if not ok then
      ngx.log(ngx.ERR, "failed to run task_infos check cycle: ", count, err)
    end
    -- keep this debug for now
    if (count % 600) == 599 then
      ngx.log(ngx.INFO, "task_infos check cycle: ", count)
    end

    local ok, err = ngx.timer.at(config.task_infos_check_period, check, count)
    if not ok then
      if err ~= "process exiting" then
        errlog("failed to create timer: ", err)
      end
    end
end

----
return task_infos

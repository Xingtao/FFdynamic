local os = require "os"
local iresty_test = require "resty.iresty_test"
local http = require "resty.http"
local config = require "config_bridge"

local unit_test = iresty_test.new({unit_name="ffdynamic test"})
local httpc = http.new()
httpc:set_timeout(500)

local ffdynamic_query_load_uri = 'http://127.0.0.1:8444/rest/v1/get_loads'

function unit_test:init()
  return
end

local test_case_suit1_room_id = "100"
local test_case_suit2_room_id = "200"

-- order matters
local test_case_suit1 = {
  ["/api1/ial/create_room/"         .. test_case_suit1_room_id] = "./unittest/test_case/create_room.json",
  ["/api1/ial/join_room/"           .. test_case_suit1_room_id] = "./unittest/test_case/join_room_hks.json",
  ["/api1/ial/room_stream_info/"    .. test_case_suit1_room_id] = "./unittest/test_case/query_stream_info.json",
  ["/api1/ial/join_room/"           .. test_case_suit1_room_id] = "./unittest/test_case/join_room_hks.json",
  ["/api1/ial/set_stream_state/"    .. test_case_suit1_room_id] = "./unittest/test_case/set_stream_state_disable.json",
  ["/api1/ial/join_room/"           .. test_case_suit1_room_id] = "./unittest/test_case/join_room_hks.json",
  ["/api1/ial/change_layout/"       .. test_case_suit1_room_id] = "./unittest/test_case/change_layout.json",
  ["/api1/ial/add_filter/"          .. test_case_suit1_room_id] = "./unittest/test_case/add_filter.json",
  ["/api1/ial/filter_chain/"        .. test_case_suit1_room_id] = "./unittest/test_case/query_filter_chains.json",
  ["/api1/ial/delete_filter/"       .. test_case_suit1_room_id] = "./unittest/test_case/delete_filter.json",
  ["/api1/ial/mute_participants/"   .. test_case_suit1_room_id] = "./unittest/test_case/mute_all.json",
  ["/api1/ial/unmute_participants/" .. test_case_suit1_room_id] = "./unittest/test_case/unmute_all.json"
  ["/api1/ial/left_room/"           .. test_case_suit1_room_id] = "./unittest/test_case/left_room_hks.json",
  ["/api1/ial/left_room/"           .. test_case_suit1_room_id] = "./unittest/test_case/left_room_hks.json",
  ["/api1/ial/left_room/"           .. test_case_suit1_room_id] = "./unittest/test_case/left_room_hks.json"
}

local test_case_suit2 = {
  ["/api1/ial/create_room/"         .. test_case_suit2_room_id] = "./unittest/test_case_2/create_room.json",
  ["/api1/ial/join_room/"           .. test_case_suit2_room_id] = "./unittest/test_case_2/join_room_hks.json",
  ["/api1/ial/room_stream_info/"    .. test_case_suit2_room_id] = "./unittest/test_case_2/query_stream_info.json",
  ["/api1/ial/join_room/"           .. test_case_suit2_room_id] = "./unittest/test_case_2/join_room_hks.json",
  ["/api1/ial/set_stream_state/"    .. test_case_suit2_room_id] = "./unittest/test_case_2/set_stream_state_disable.json",
  ["/api1/ial/join_room/"           .. test_case_suit2_room_id] = "./unittest/test_case_2/join_room_hks.json",
  ["/api1/ial/change_layout/"       .. test_case_suit2_room_id] = "./unittest/test_case_2/change_layout.json",
  ["/api1/ial/add_filter/"          .. test_case_suit2_room_id] = "./unittest/test_case_2/add_filter.json",
  ["/api1/ial/filter_chain/"        .. test_case_suit2_room_id] = "./unittest/test_case_2/query_filter_chains.json",
  ["/api1/ial/delete_filter/"       .. test_case_suit2_room_id] = "./unittest/test_case_2/delete_filter.json",
  ["/api1/ial/mute_participants/"   .. test_case_suit2_room_id] = "./unittest/test_case_2/mute_all.json",
  ["/api1/ial/unmute_participants/" .. test_case_suit2_room_id] = "./unittest/test_case_2/unmute_all.json"
  ["/api1/ial/left_room/"           .. test_case_suit2_room_id] = "./unittest/test_case_2/left_room_hks.json",
  ["/api1/ial/left_room/"           .. test_case_suit2_room_id] = "./unittest/test_case_2/left_room_hks.json",
  ["/api1/ial/left_room/"           .. test_case_suit2_room_id] = "./unittest/test_case_2/left_room_hks.json"
}

function unit_test:test_ial_test_suit_1()
  Self:log("start ial_test_suit_1 test")
  for k, v in ipairs(test_case_suit_1) do
    local f = io.open(v, "r")
    local testcase_content = f:read("*a")
    f:close()
    local start_time = os.time()
    local resp, err = httpc:request_uri("http://127.0.0.1:" .. tostring(config.bridge_port) .. k, {method = "POST"})
    local end_time = os.time()
    self.log(k, " consumes ", os.difftime(end_time, start_time))
    if not resp or resp.status ~= 200 then
      error("ial_test_suit_1 fail, " .. (err or ""), k, v)
    end
    self:log("ial_test_suit_1 " .. k .. " pass: " .. (resp.body or ""))
  end
  self:log("finish ial_test_suit_1 test")
end

-- run the test
unit_test:run()

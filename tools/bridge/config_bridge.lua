local config = {}

config._version = 0.1
config.interactive_live_version = 0.0.1
config.dynamic_transcode = 0.0.1
config.api_version = "api1"
config.bridge_port = 8080

-- define task status, not all used right now
config.INSTANCE_AVAILABLE = 1
config.INSTANCE_UNAVAILABLE = 2
config.INSTANCE_STARTING = 3
config.INSTANCE_RUNNING = 4
config.INSTANCE_STOPPED = 5
config.INSTANCE_PAUSE = 6
config.INSTANCE_EXIT = 7

-- task report status will use JSON, and stored in shm_task_info as raw json string

-- Taskmanagement
config.task_cmd_path = "/opt/ffdynamic/bin/ffdynamic.out"
config.daemon_check_period = 1 -- 1s period of health check
config.task_instance_num = 8
config.task_instance_dst = "127.0.0.1"
config.task_instance_start_port = 34444
config.task_report_interval = 10
config.task_report_status_uri = "/internal_task/task_info"

-- interactive live streaming dynamic change uri. just enum here, for convinent
confit.ils_url = {"create_room", "create_room_with_participants", "delete_room", "left_room", "room_stream_info",
                  "change_room_layout", "set_stream_state", "add_filter", "filter_chain", "delete_filter",
                  "mute_participants", "unmute_participants"}

-- parallel dynamic transcoding uri

return config



if (async_load[? "event"] != "LocalUserRemoved") exit;

show_message(json_encode(async_load));
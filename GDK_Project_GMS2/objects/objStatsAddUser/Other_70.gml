

if (async_load[? "event"] != "LocalUserAdded") exit;

show_message(json_encode(async_load));
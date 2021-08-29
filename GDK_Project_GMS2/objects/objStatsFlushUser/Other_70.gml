

if (async_load[? "event"] != "StatisticUpdateComplete") exit;

show_message(json_encode(async_load));
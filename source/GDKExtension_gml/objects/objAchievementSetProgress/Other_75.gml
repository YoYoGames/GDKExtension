

if (async_load[?"event_type"] != "achievement result") exit;

if (async_load[?"requestID"] != requestId) exit;

show_message(json_encode(async_load));
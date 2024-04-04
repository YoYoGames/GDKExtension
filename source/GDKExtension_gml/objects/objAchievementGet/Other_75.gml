

if (async_load[?"event_type"] != "achievement info") exit;

if (async_load[?"requestID"] != requestId) exit;

show_message(json_encode(async_load));

// available data:
// async_load[? "event_type"]
// async_load[? "requestID"]
// async_load[? "achievement"]
// async_load[? "name"]
// async_load[? "progress_state"]
// async_load[? "progress"]

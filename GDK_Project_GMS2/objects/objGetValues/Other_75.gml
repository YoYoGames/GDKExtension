/// @description Insert description here
// You can write your code in this editor

if (async_load[? "event_type"] != "stat_result") exit;

show_debug_message(json_encode(async_load));

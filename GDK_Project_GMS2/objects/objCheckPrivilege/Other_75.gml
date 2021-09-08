/// @description Insert description here
// You can write your code in this editor

if (async_load[? "event_type"] != "check_privilege_result") exit;

show_debug_message(json_encode(async_load));
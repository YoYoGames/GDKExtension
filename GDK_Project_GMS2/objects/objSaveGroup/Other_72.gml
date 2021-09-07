/// @description Insert description here
// You can write your code in this editor

var _id = async_load[? "id"];

if(requestId != async_load[? "id"]) exit;

show_debug_message(json_encode(async_load));

var _error = async_load[? "error"];

if (_error != 0) show_debug_message("[ERROR] gdk_save_buffer (group)");
else show_debug_message("[SUCCESS] gdk_save_buffer (group)");
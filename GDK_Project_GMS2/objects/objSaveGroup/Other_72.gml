/// @description Insert description here
// You can write your code in this editor

var _id = async_load[? "id"];

if(async_load[? "type"] == "gdk_save_group_end_result")
{
	show_debug_message(json_encode(async_load));
	exit;
}

if (!variable_struct_exists(requestIds, _id)) exit;

variable_struct_remove(requestIds, _id);

var _error = async_load[? "error"];
errorFlag |= bool(_error);

show_debug_message(json_encode(async_load));

if (variable_struct_names_count(requestIds) == 0) {
	if (!errorFlag) show_debug_message("[SUCCESS] gdk_save_buffer (group)");
	else show_debug_message("[ERROR] gdk_save_buffer (group)");
}
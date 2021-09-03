/// @description Insert description here
// You can write your code in this editor

var _id = async_load[? "id"];

if (!variable_struct_exists(requestIds, _id)) exit;

var _requestInfo = requestIds[$ _id];
variable_struct_remove(requestIds, _id);

var _error = async_load[? "error"];
errorFlag |= bool(_error);

var _filename = _requestInfo.filename;
var _bufferId = _requestInfo.bufferId;

var _data = buffer_read(_bufferId, buffer_text);

show_debug_message(json_encode(async_load));
show_debug_message("Loaded '" + _data + "' from '" + _filename + "'");

buffer_delete(_bufferId);

if (variable_struct_names_count(requestIds) == 0) {
	if (!errorFlag) show_debug_message("[SUCCESS] gdk_load_buffer");
	else show_debug_message("[ERROR] gdk_load_buffer");
}

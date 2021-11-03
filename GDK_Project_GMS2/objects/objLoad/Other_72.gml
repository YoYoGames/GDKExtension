/// @description Insert description here
// You can write your code in this editor

var _id = async_load[? "id"];

// Early exit if we don't need to handle this requestId.
if (!variable_struct_exists(requestIds, _id)) exit;

// Remove requestId from queue struct
var _requestInfo = requestIds[$ _id];
variable_struct_remove(requestIds, _id);

// This line covers both Windows GDK (error) and Xbox Series X/S (status) 
var _error = async_load[? "error"] == true || async_load[? "status"] == false;

// If we have an error we keep it in a flag
errorFlag |= bool(_error);

var _filename = _requestInfo.filename;
var _bufferId = _requestInfo.bufferId;

var _data = buffer_read(_bufferId, buffer_text);

// Log loaded data from buffer.
show_debug_message(json_encode(async_load));
show_debug_message("Loaded '" + _data + "' from '" + _filename + "'");

buffer_delete(_bufferId);

// When we processed all the buffers load requests
if (variable_struct_names_count(requestIds) == 0) {
	// Print status (SUCCESS/ERROR)
	if (!errorFlag) show_debug_message("[SUCCESS] gdk_load_buffer");
	else show_debug_message("[ERROR] gdk_load_buffer");
}

/// @description Insert description here
// You can write your code in this editor

var _id = async_load[? "id"];

if(requestId != async_load[? "id"]) exit;

show_debug_message(json_encode(async_load));


switch (os_type) {
	case os_windows:

		// On Windows GDK the async load maps comes with a 'error' that
		// will let you know if anything when wrong with the save process.
		var _error = async_load[? "error"];

		if (_error != 0) show_debug_message("[ERROR] gdk_save_buffer (group)");
		else show_debug_message("[SUCCESS] gdk_save_buffer (group)");
		break;

	case os_xboxseriesxs:
	
		// On Xbox Series X/S GDK the async_load maps comes with a 'status' that
		// will let you know if anything when wrong with the save process.
		var _status = async_load[? "status"];

		if (_status == false) show_debug_message("[ERROR] buffer_save_async (group)");
		else show_debug_message("[SUCCESS] buffer_save_async (group)");
		break;
		
	default: throw "[ERROR] objSaveGroup, unsupported platform";
}

requestId = noone;

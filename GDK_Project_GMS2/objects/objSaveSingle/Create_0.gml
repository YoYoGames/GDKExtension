/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Save (single)";
requestId = noone;

onClick = function() {

	var _userId = xboxone_get_savedata_user();
	show_debug_message("[INFO] gdk_save_buffer (userID: " + string(_userId) + ")");
	
	/* Save a single buffer. */

	var b1 = buffer_create(1, buffer_grow, 1);
	buffer_write(b1, buffer_text, "foo");

	switch (os_type) {
		
		case os_windows:
			// On Windows GDK the function used to save buffers asynchronously
			// is 'gdk_save_buffer' it will return a requestID that can be checked
			// during the Async Save/Load event.
			requestId = gdk_save_buffer(b1, "single/b1", 0, buffer_tell(b1));
			break;
			
		case os_xboxseriesxs:
			// On Xbox Series X/S the function used to save buffers asynchronously
			// is 'buffer_save_async' it will return a requestID that can be checked
			// during the Async Save/Load event.
			requestId = buffer_save_async(b1, "single/b1", 0, buffer_tell(b1));
			break;
			
		default: throw "[ERROR] objSaveSingle, unsupported platform";
	}
	
	
	if (requestId == -1) show_debug_message("[ERROR] gdk_save_buffer");
	
	buffer_delete(b1);
}

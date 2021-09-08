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

	requestId = gdk_save_buffer(b1, "single/b1", 0, buffer_tell(b1));
	if (requestId == -1) show_debug_message("[ERROR] gdk_save_buffer");
	
	buffer_delete(b1);
}

/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Save (single)";
requestId = noone;

onClick = function() {
	var user_id = xboxone_get_activating_user();
	xboxone_set_savedata_user(user_id);
	
	/* Save a single buffer. */

	var b1 = buffer_create(1, buffer_grow, 1);
	buffer_write(b1, buffer_text, "foo");

	requestId = gdk_save_buffer(b1, "single/b1", 0, buffer_tell(b1));
	if (requestId == -1) show_debug_message("[ERROR] gdk_save_buffer");
	
	buffer_delete(b1);
}

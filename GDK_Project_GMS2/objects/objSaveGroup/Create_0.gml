/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Save (group)";
requestId = noone;

onClick = function() {
	
	var _userId = xboxone_get_savedata_user();
	show_debug_message("[INFO] gdk_save_buffer (userID: " + string(_userId) + ")");

	/* Save a group of buffers. */
	
	var b2 = buffer_create(1, buffer_grow, 1);
	buffer_write(b2, buffer_text, "bar");
	
	var b3 = buffer_create(1, buffer_grow, 1);
	buffer_write(b3, buffer_text, "baz");
	
	var b4 = buffer_create(1, buffer_grow, 1);
	buffer_write(b4, buffer_text, "qux");
	
	gdk_save_group_begin("multi");
	gdk_save_buffer(b2, "b2", 0, buffer_tell(b2));
	gdk_save_buffer(b3, "b3", 0, buffer_tell(b3));
	gdk_save_buffer(b4, "b4", 0, buffer_tell(b4));
	requestId = gdk_save_group_end();
	
	buffer_delete(b2);
	buffer_delete(b3);
	buffer_delete(b4);
}
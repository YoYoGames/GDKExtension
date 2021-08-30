/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Save data";

onClick = function() {
	var user_id = xboxone_get_activating_user();
	xboxone_set_savedata_user(user_id);
	
	/* Save a single buffer. */
	
	var b1 = buffer_create(1, buffer_grow, 1);
	buffer_write(b1, buffer_text, "foo");
	
	gdk_save_buffer(b1, "single/b1", 0, buffer_tell(b1));
	
	buffer_delete(b1);
	
	/* Save several buffers in the same transaction. */
	
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
	
	gdk_save_group_end();
}

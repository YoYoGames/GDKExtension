/// @description Insert description here
// You can write your code in this editor

/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Load";
requests = [];

onClick = function() {
	
	var _userId = xboxone_get_savedata_user();
	if (_userId == pointer_null) {
		show_debug_message("[INFO] gdk_load_buffer (null user, setting to default user)");
		_userId = xboxone_get_activating_user();
		xboxone_set_savedata_user(_userId);
	}
	
	queue_load("single/b1");
	queue_load("multi/b2");
	queue_load("multi/b3");
	queue_load("multi/b4");
}

function queue_load(filename)
{
	var buf_id = buffer_create(1, buffer_grow, 1);
	var req_id = gdk_load_buffer(buf_id, filename, 0, 10);
	
	array_push(requests, {
		filename: filename,
		buf_id: buf_id,
		req_id: req_id,
	});
}

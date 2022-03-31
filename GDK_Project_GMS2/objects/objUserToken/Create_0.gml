/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "User Token";
onClick = function() {	
	var headers = ds_map_create();
	headers[?"this"] = "that";
	headers[?"woot"] = "this";

	var body = buffer_create(1, buffer_grow, 1);
	buffer_write(body, buffer_string, "hello");
	
	var url = "https://profile.xboxlive.com/users/me/profile/settings?settings=GameDisplayName";
	var request_method = "GET";

	if (xboxone_get_token_and_signature(xboxone_get_activating_user(), url, request_method, json_encode(headers), true, body) < 0){
		show_message("Token query failed");
	}
	else{
		show_debug_message("Requested token.");
	}
	
	buffer_delete(body);
	ds_map_destroy(headers);	
}
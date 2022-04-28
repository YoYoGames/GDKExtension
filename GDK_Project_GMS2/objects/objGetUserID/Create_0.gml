/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();


text = os_type == os_windows ? "ID from Index" :  "ID from Index (*)";
requestId = noone;

onClick = function() {
	requestId = get_integer_async("User index: ", 0);
}

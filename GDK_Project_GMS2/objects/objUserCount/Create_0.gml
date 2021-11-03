/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "User Count";

onClick = function() {
	var _count = xboxone_get_user_count();
	show_message("Number of connected users: " + string(_count));
}
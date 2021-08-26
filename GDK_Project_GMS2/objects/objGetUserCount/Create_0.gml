/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Get User Count"

onClick = function() {
	var _count = xboxone_get_user_count();
	show_message(_count);
}
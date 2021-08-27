/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Remove User";

onClick = function() {
	var _userId = xboxone_get_user(0);
	xboxone_stats_remove_user(_userId);
}
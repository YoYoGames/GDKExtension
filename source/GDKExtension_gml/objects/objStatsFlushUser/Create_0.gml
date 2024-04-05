/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Flush (sync)";

onClick = function() {
	var _userId = xboxone_get_user(0);
	xboxone_stats_flush_user(_userId, false);
}
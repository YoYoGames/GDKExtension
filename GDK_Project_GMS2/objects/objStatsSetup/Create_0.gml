/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Stats Setup";

onClick = function() {
	
	var user_one = xboxone_get_user(0);
	xboxone_stats_setup(user_one, SCID, $6AF77944);
	
	show_debug_message("[INFO] Event-based stats are not fully implemented yet.");
}
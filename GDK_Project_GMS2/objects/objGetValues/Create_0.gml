/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Get Values";

onClick = function() {
	
	var _userId = xboxone_get_user(0);
	var _error = xboxone_get_stats_for_user(_userId, SCID, "TestInt", "TestReal", "TestString");
	
	if (_error != 0) show_debug_message("[ERROR] xboxone_get_stats_for_user");
	else show_debug_message("[SUCCESS] xboxone_get_stats_for_user");
}
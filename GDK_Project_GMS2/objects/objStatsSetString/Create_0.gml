/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Set 'TestString'";

onClick = function() {
	var _userId = xboxone_get_user(0);
	
	var _string = choose("YoYoGames", "GameMaker Studio", "Opera GX");
	var _error = xboxone_stats_set_stat_string(_userId, "TestString", _string);
	
	if (_error == -1) show_debug_message("[ERROR] xboxone_stats_set_stat_string");
	else show_debug_message("[SUCCESS] xboxone_stats_set_stat_string, new value: " + _string);
}
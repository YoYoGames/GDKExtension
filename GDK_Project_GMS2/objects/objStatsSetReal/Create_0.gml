/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Set 'TestReal'";

onClick = function() {
	var _userId = xboxone_get_user(0);
	
	var _real = random(999);
	var _error = xboxone_stats_set_stat_real(_userId, "TestReal", _real);
	
	if (_error == -1) show_debug_message("[ERROR] xboxone_stats_set_stat_real");
	else show_debug_message("[SUCCESS] xboxone_stats_set_stat_real, new value: " + string(_real));
}
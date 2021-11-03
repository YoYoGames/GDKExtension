/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Set 'Int'";

// This function is called when you click the button.
onClick = function() {
	var _userId = xboxone_get_user(0);
	
	var _int = irandom(999);
	var _error = xboxone_stats_set_stat_int(_userId, "Int", _int);
	
	if (_error == -1) show_debug_message("[ERROR] xboxone_stats_set_stat_int");
	else show_debug_message("[SUCCESS] xboxone_stats_set_stat_int, new value: " + string(_int));
}
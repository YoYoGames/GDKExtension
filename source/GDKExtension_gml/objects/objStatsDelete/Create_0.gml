/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "X";
requestId = noone;

// This is the name of the stat to be deleted.
// NOTE: Defined inside the creation code of each 'objStatsDelete'.
statName = "statName";

// This function is called when you click the button.
onClick = function() {
	var _userId = xboxone_get_user(0);
	var _error = xboxone_stats_delete_stat(_userId, statName);

	if (_error == -1) show_debug_message("[ERROR] xboxone_stats_delete_stat");
	else show_debug_message("[SUCCESS] xboxone_stats_delete_stat");
}

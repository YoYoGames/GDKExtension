/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Submit";

onClick = function() {

	// Create a random value for the highscore based on the time passed
	var _secondsSinceStart = get_timer() * 0.000001;
	var _value = irandom(_secondsSinceStart * 20);

	// Setting our active users 'Highscore' stat.
	// Note that REGULAR stats can be used for SOCIAL leaderboards only.
	var _userId = xboxone_get_user(0);
	var _error = xboxone_stats_set_stat_int(_userId, "Highscore", _value);
	
	// Log debug information.
	if (_error == -1) show_debug_message("[ERROR] xboxone_stats_set_stat_int");
	else show_debug_message("[SUCCESS] xboxone_stats_set_stat_int, new value: " + string(_value));
}
/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Submit";

onClick = function() {

	// Get seconds since start
	var _value = get_timer() * 0.000001;

	// Setting our active users 'TimeRecord' featured stat.
	// Note that FEATURED stats must be configured through the Partner Center
	// and can be used for GLOBAL leaderboards, unlike regular stats.
	var _userId = xboxone_get_user(0);
	var _error = xboxone_stats_set_stat_real(_userId, "TimeRecord", _value);
	
	// Log debug information.
	if (_error == -1) show_debug_message("[ERROR] xboxone_stats_set_stat_real");
	else show_debug_message("[SUCCESS] xboxone_stats_set_stat_real, new value: " + string(_value));
}
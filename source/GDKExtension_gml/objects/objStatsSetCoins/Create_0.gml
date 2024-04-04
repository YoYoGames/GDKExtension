/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Submit";

// This function is called when you click the button.
onClick = function() {

	// Create a random value for the coins collected
	// based on the time passed
	var _secondsSinceStart = get_timer() * 0.000001;
	var _value = round(_secondsSinceStart * 2);

	// Setting our active users 'CoinsCollected' featured stat.
	// Note that FEATURED stats must be configured through the Partner Center
	// and can be used for GLOBAL leaderboards, unlike regular stats.
	var _userId = xboxone_get_user(0);
	var _error = xboxone_stats_set_stat_int(_userId, "CoinsCollected", _value);
	
	// Log debug information.
	if (_error == -1) show_debug_message("[ERROR] xboxone_stats_set_stat_int");
	else show_debug_message("[SUCCESS] xboxone_stats_set_stat_int, new value: " + string(_value));

}
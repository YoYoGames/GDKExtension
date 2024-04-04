/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Values";

// This function is called when you click the button.
onClick = function() {
	var _userId = xboxone_get_user(0);
	var _statNames = xboxone_stats_get_stat_names(_userId);
	
	var _output = {};
	var _count = array_length(_statNames);
	repeat(_count) {
		var _statName = _statNames[--_count];
		
		_output[$ _statName] = xboxone_stats_get_stat(_userId, _statName);
	}
	
	show_message(_output);
}
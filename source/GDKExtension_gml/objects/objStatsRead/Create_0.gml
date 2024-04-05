/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "R";
requestId = noone;

// Defined inside the creation code of each 'objStatsRead'.
statName = "statName";

// This function is called when you click the button.
onClick = function() {
	var _userId = xboxone_get_user(0);
	var _value = xboxone_stats_get_stat(_userId, statName);
	
	show_message("The returned value was: '" + string(_value) + "'");
}
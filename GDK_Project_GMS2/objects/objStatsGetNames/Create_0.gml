/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Get Names";

onClick = function() {
	var _userId = xboxone_get_user(0);
	var _statNames = xboxone_stats_get_stat_names(_userId);
	show_message(_statNames);
}
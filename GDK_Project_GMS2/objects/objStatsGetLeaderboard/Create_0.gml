/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Leaderboard";

onClick = function() {
	var _userId = xboxone_get_activating_user();
	xboxone_stats_get_leaderboard(_userId, "GlobalTime", 10, 0, true, true);
}
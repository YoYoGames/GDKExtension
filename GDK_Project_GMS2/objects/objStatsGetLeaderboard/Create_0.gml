/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Fetch";

onClick = function() {
	var _userId = xboxone_get_activating_user();
	xboxone_stats_get_leaderboard(_userId, "HighScore", 10, 0, true, true);
}

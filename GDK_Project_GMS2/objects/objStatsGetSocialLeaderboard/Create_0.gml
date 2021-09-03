/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Fetch (Social)";

onClick = function() {
	var _userId = xboxone_get_activating_user();
	xboxone_stats_get_social_leaderboard(_userId, "TestInt", 10, 0, true, true, true);
}
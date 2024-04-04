/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Fetch";
leaderboardID = "leaderboardID";

onClick = function() {
	var _userId = xboxone_get_user(0);	
	xboxone_stats_get_social_leaderboard(_userId, leaderboardID, 10, 0, true, true, false);

	// The async event for this function call is inside the 'objLeaderboardAsync'
	// since both leaderboard queries share the same Async Social event and type.
}
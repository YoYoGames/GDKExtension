/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Read Leaderboard";

onClick = function() {
	var _userId = xboxone_get_savedata_user();	
	var _userName = xboxone_user_id_for_user(_userId);
	
	xboxone_read_player_leaderboard("HighScore", _userName, 5, achievement_filter_favorites_only);
}
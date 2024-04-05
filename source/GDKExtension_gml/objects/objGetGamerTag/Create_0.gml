/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Gamer Tag"

presenceId = "presenceId";

onClick = function() {
	var _userId = xboxone_get_activating_user();
	var _gamerTag = xboxone_gamertag_for_user(_userId);
	
	show_message("Your gamer tag is: '" + _gamerTag + "'");
}
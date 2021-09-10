/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Rich Presence"

presenceId = "mainMenu";

onClick = function() {
	var _userId = xboxone_get_activating_user();
	xboxone_set_rich_presence(_userId, true, presenceId, SCID);
}
/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Get Workspace";

onClick = function() {
	var _userId = xboxone_get_savedata_user();
	show_message("Saving to UserID: " + string(_userId));
}
/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Activating UserID"

onClick = function() {
	var _userId = xboxone_get_activating_user();
	show_message(_userId);
}
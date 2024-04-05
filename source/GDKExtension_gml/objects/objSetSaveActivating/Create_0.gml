/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Set to Activating";

onClick = function() {
	var _userId = xboxone_get_activating_user();
	var _error = xboxone_set_savedata_user(_userId);
	if (_error == -1) show_debug_message("[ERROR] xboxone_set_savedata_user");
	else show_debug_message("[SUCCESS] xboxone_set_savedata_user");
}
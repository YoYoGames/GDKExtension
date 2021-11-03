/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Set to Local";

onClick = function() {
	var _error = xboxone_set_savedata_user(pointer_null);
	if (_error == -1) show_debug_message("[ERROR] xboxone_set_savedata_user");
	else show_debug_message("[SUCCESS] xboxone_set_savedata_user");
}
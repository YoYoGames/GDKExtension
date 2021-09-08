/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Check Previlege"
requestId = noone;

onClick = function() {
	var _userId = xboxone_get_activating_user();
	xboxone_check_privilege(_userId, xboxone_privilege_multiplayer_sessions, true);
}
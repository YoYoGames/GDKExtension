/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Account Picker"
requestId = noone;


onClick = function() {
	requestId = xboxone_show_account_picker(0, 0);
	show_message(requestId)
}
/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Goto ???";
target = undefined;

onClick = function() {
	// Set 'text' and 'target' in creation code.
	if (target == undefined) exit;
	room_goto(target);
}
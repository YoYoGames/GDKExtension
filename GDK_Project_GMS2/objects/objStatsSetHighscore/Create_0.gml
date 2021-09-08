/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Submit";
requestId = noone;

onClick = function() {
	requestId = get_integer_async("New Highscore: ", 0);
}
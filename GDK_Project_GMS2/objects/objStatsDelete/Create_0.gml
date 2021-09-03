/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Delete";
requestId = noone;

onClick = function() {
	requestId = get_string_async("Stat Name (TestInt/TestReal/TestString): ", "TestInt");
}

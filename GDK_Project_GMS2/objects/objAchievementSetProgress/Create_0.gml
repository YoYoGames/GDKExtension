/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Increment 10%"
requestId = noone;

progress = 0;
achievementId = "1";

onClick = function() {
	var _userId = xboxone_get_activating_user();
	
	progress += 10;
	requestId = xboxone_achievements_set_progress(_userId, achievementId, progress);
}
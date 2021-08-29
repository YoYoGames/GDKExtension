/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Achievement 1 + 10%"

achievement_progress = 0;

onClick = function() {
	var _userId = xboxone_get_activating_user();
	
	achievement_progress += 10;
	show_debug_message("Setting Test Achivement 1 completion to " + string(achievement_progress) + "%");
	xboxone_achievements_set_progress(_userId, "1", achievement_progress);
}


/// @description CODE IS HERE

// Inherit the parent event
event_inherited();

progress = 0;
achievementId = "1";

text = "1";
requestId = noone;

// This function is called on button click
onClick = function() {
	var _userId = xboxone_get_user(0);
	
	progress += 10;
	// This call triggers an Async System call (check event for more info)
	requestId = xboxone_achievements_set_progress(_userId, achievementId, progress);
}
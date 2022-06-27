/// @description CODE IS HERE

// Inherit the parent event
event_inherited();

achievementId = "1";

text = "1";
requestId = noone;

// This function is called on button click
onClick = function() {
	var _userId = xboxone_get_user(0);
	
	// This call triggers an Async System call (check event for more info)
	requestId = xboxone_get_achievement(_userId, achievementId);
}
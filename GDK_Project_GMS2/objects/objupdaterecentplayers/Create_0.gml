/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

recent_players = 0123456789012345;

text = "Update recent players (one ID)";

onClick = function() {	
	
	var success = xboxone_update_recent_players(xboxone_get_activating_user(), recent_players);	
	
	if (success == 0)
	{
		show_message("Recent player list updated successfully");
	}
	else
	{
		show_message("Recent player list update failed");
	}
}
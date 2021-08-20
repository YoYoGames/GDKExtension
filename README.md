# GameMaker Studio 2 - GDK Extension

An Extension for GameMaker Studio 2 (GMS2) that gives GMS2 Windows Target support for the GDK allowing them to be released on the Microsoft Store and use XBox Live functionality (for those developers that have access through id@xbox, see INSERTLINK  for more information on id@xbox ).

NOTE: Only Windows x64 Target is supported by the GDK, ensure that your GMS2 project has the x64 option selected in Options -> Windows -> General

## Contents of this repository

This repository contains the source code for the DLL that implements the GDK functionality that is exposed to GameMaker games, it is written in C++. 

It also contains an example GMS2 project that contains the extension definition and illustrates how to use the extension.

## Building this Extension


	1. Install VS2019 - https://visualstudio.microsoft.com/downloads/ 
	2. Install see https://github.com/microsoft/GDK (clone repository then run the installer)
	3. Install CMAKE - see https://cmake.org/download/
	3. Clone this repository (NOTE: This repository has submodules)
	4. Open the Visual Studio 2019
	5. Open the Solution in DLL/GDKExtension.sln
	6. Select the Debug|Gaming.Desktop.x64 [or Release|Gaming.Desktop.x64 configurations COMING SOON]
	7. Build

NOTE: Output from this build will be copied into the GMS2 GDK  project


## Running the GMS2 Project

Open the GMS2 Project in this repository from GDK_Project_GMS2/GDK_Project_GMS2.yyp file.


# GDK Extension GML Function reference

NOTE: these functions should work the same as those in the xbox one and xbox series x/s targets and we have adopted the same naming so game code can be shared with the extension.

## gdk_init  - Initialise the GDK

usage: gdk_init()

returns: nothing

description: Must be called before any other GDK extension function, recommend using a Controller persistent object that is created or placed in the first room and this call is in the create event.


## gdk_update - Update the GDK

usage: gdk_update()

returns: nothing

description: This should be called each frame while GDK extension is active, recommend using a Controller persistent object that is created or placed in the first room and this call is in the step event.

## gdk_quit - Quit the GDK

usage: gdk_quit()

returns: nothing

description: This should be called on close down and no GDK extension functions should be called after it, recommend using a Controller persistent object that is created or placed in the first room and this call is in the destroy event.


## xboxone_show_account_picker 

usage: xboxone_show_account_picker( arg0, arg1 )

return: nothing

description: show the GDK account picker

## xboxone_get_user_count - xboxone_get_user_count()
## xboxone_get_user - xboxone_get_user(index)
## xboxone_get_activating_user - xboxone_get_activating_user()
## xboxone_fire_event - xboxone_fire_event(event_name, ...)
## xboxone_get_stats_for_user - xboxone_get_stats_for_user(user_id, scid)
## xboxone_stats_setup - xboxone_stats_setup(user_id, scid, title_id)
## xboxone_stats_set_stat_real - xboxone_stats_set_stat_real(user_id, stat_name, stat_value)
## xboxone_stats_set_stat_int - xboxone_stats_set_stat_int(user_id, stat_name, stat_value)
## xboxone_stats_set_stat_string - xboxone_stats_set_stat_string(user_id, stat_name, stat_value)
## xboxone_stats_delete_stat - xboxone_stats_delete_stat(user_id, stat_name)
## xboxone_stats_get_stat - xboxone_stats_get_stat(user_id, stat_name)
## xboxone_stats_get_stat_names - xboxone_stats_get_stat_names(user_id)
## xboxone_stats_add_user - xboxone_stats_add_user(user_id)
## xboxone_stats_remove_user - xboxone_stats_remove_user(user_id)
## xboxone_stats_flush_user - xboxone_stats_flush_user(user_id, high_priority)
## xboxone_stats_get_leaderboard - xboxone_stats_get_leaderboard(user_id, stat, num_entries, start_rank, start_at_user, ascending)
## xboxone_stats_get_social_leaderboard - xboxone_stats_get_social_leaderboard(user_id, stat, num_entries, start_rank, start_at_user, ascending, favourites_only)
## xboxone_achievements_set_progress - xboxone_achievements_set_progress(user_id, achievement, progress)
## xboxone_read_player_leaderboard - xboxone_read_player_leaderboard(ident, player, numitems, friendfilter)
## xboxone_set_rich_presence - xboxone_set_rich_presence(user_id, is_user_active, rich_presence_string)
## xboxone_check_privilege - xboxone_check_privilege(user_id, privilege_id, attempt_resolution)


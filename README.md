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

---

## gdk_init  - Initialise the GDK

**Usage**: gdk_init(scid)

**Description**: Must be called before any other GDK extension function, recommend using a Controller persistent object that is created or placed in the first room and this call is in the create event.

**Params**:

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*string*} **scid** The Service Configuration ID (SCID) presented in the MicrosoftGame.config file.

**Returns**: N/A

---

## gdk_update - Update the GDK

**Usage**: gdk_update()

**Description**: This should be called each frame while GDK extension is active, recommend using a Controller persistent object that is created or placed in the first room and this call is in the step event.

**Returns**: N/A

---

## gdk_quit - Quit the GDK

**Usage**: gdk_quit()

**Description**: This should be called on close down and no GDK extension functions should be called after it, recommend using a Controller persistent object that is created or placed in the first room and this call is in the destroy event.

**Returns**: N/A

---

## xboxone_show_account_picker (TODO)

**Usage**: xboxone_show_account_picker( arg0, arg1 )

**Description**: This function launches the system’s account picker which will associate the selected user with the specified pad. The "mode" argument is either 0 or 1 – if 0 is specified no guest accounts can be selected, while 1 allows guest accounts. This function returns an asynchronous event ID value, and when the account picker closes an **asynchronous dialog event** will be triggered with details of the result of the operation.

**Triggers**: Async Dialog Event

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*real*} **"id"**: The event ID.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*string*} **"type"**: A string containing "xboxone_accountpicker".

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*real*} **"padindex"**: Contains the pad index passed in to the function.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*id*} **"user"**: Contains the User ID (a pointer) returned by the account picker (or null pointer if cancelled).

**Returns**: {*real*} The asynchronous event ID value.

---

## xboxone_get_user_count

**Usage**: xboxone_get_user_count()

**Description**: With this function you can find the total number of users currently signed in to the system. The returned value will be an integer value.

**Returns**: {*integer*} The total number of users currently signed in to the system.

**Code Sample**:

	for (var i = 0; i < xboxone_get_user_count(); i++;) {
		user_id[i] = xboxone_get_user(i);
	}

---

## xboxone_get_user

**Usage**: xboxone_get_user(index)

**Description**: With this function you can retrieve the user ID pointer for the indexed user. If the user does not exist, the function will return the constant pointer_null instead. You can find the number of users currently logged in with the function [`xboxone_get_user_count()`](#xboxone_get_user_count).

**Params**:

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*integer*} **index** The index (an integer) to get the User ID from.

**Returns**: {*integer*} The total number of users currently signed in to the system.

**Code Sample**:

	for (var i = 0; i < xboxone_get_user_count(); i++;) {
		user_id[i] = xboxone_get_user(i);
	}

---

## xboxone_get_activating_user

**Usage**: xboxone_get_activating_user()

**Description**: With this function you can retrieve the user ID pointer for the user that launched the game from the console. Note that this is rarely what you want to do in a game, because this user can logout while the game is running. 

**Returns**: {*pointer*} The user ID pointer.

**Code Sample**:

	global.main_user = xboxone_get_activating_user();

---

## xboxone_fire_event - xboxone_fire_event(event_name, ...)
## xboxone_get_stats_for_user - xboxone_get_stats_for_user(user_id, scid)
## xboxone_stats_setup - xboxone_stats_setup(user_id, scid, title_id)
## xboxone_stats_set_stat_real - xboxone_stats_set_stat_real(user_id, stat_name, stat_value)
## xboxone_stats_set_stat_int - xboxone_stats_set_stat_int(user_id, stat_name, stat_value)
## xboxone_stats_set_stat_string - xboxone_stats_set_stat_string(user_id, stat_name, stat_value)
## xboxone_stats_delete_stat - xboxone_stats_delete_stat(user_id, stat_name)
## xboxone_stats_get_stat - xboxone_stats_get_stat(user_id, stat_name)

---

## xboxone_stats_get_stat_names (BUG)

**Usage**: xboxone_stats_get_stat_names(user_id)

**Description**: This function can be used to retrieve all the defined stats from the stat manager for a given user. You supply the user ID as returned by the function [`xboxone_get_user()`](#xboxone_get_user), and the function will returns an array of strings containing the statistics for the user. If an error occurs or the user has no stats the array will still be returned but will have an element count of 0.

**Returns**: {*array*} Array with all the stat names for the given user.

**Code Sample**:

    var _stat_str = xboxone_stats_get_stat_names(user_id);
    for (var i = 0; i < array_length_1d(_stat_str); i++;) {
        xboxone_stats_delete_stat(user_id, _stat_str[i]);
    }

---

## xboxone_stats_add_user (BUG)

**Usage**: xboxone_stats_add_user(user_id)

**Description**: This function can be used to add a given user to the statistics manager. This must be done before using any of the other stats functions to automatically sync the game with the xbox live server and retrieve the latest values. You supply the user ID as returned by the function [`xboxone_get_user()`](#xboxone_get_user), and the function will return -1 if there was an error or the user ID is invalid, or any other value if the function was successfully called.

**Triggers**: Social Asynchronous Event

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*constant*} **"id"**: Will hold the constant achievement_stat_event.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*string*} **"event"**: Will hold the string "LocalUserAdded".

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*real*} **"userid"**: The user ID associated with the request.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*real*} **"error"**: 0 if successful, some other value if there has been an error.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{*string*} **"errorMessage"**: A string with an error message, if any is available (this key is only present when there is an error)

**Returns**: {*real*} The asynchronous event ID value.

---

## xboxone_stats_remove_user - xboxone_stats_remove_user(user_id)
## xboxone_stats_flush_user - xboxone_stats_flush_user(user_id, high_priority)
## xboxone_stats_get_leaderboard - xboxone_stats_get_leaderboard(user_id, stat, num_entries, start_rank, start_at_user, ascending)
## xboxone_stats_get_social_leaderboard - xboxone_stats_get_social_leaderboard(user_id, stat, num_entries, start_rank, start_at_user, ascending, favourites_only)
## xboxone_achievements_set_progress - xboxone_achievements_set_progress(user_id, achievement, progress)
## xboxone_read_player_leaderboard - xboxone_read_player_leaderboard(ident, player, numitems, friendfilter)
## xboxone_set_rich_presence - xboxone_set_rich_presence(user_id, is_user_active, rich_presence_string)
## xboxone_check_privilege - xboxone_check_privilege(user_id, privilege_id, attempt_resolution)


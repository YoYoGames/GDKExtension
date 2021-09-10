
## `xboxone_stats_setup`

> ### **Usage**

`xboxone_stats_setup(user_id, scid, title_id)`

> ### **Description**

This function needs to be called before you can use any of the other Xbox stat functions, and simply initialises the required libraries on the system. The "xb_user" argument is the raw user ID as returned by the function [`xboxone_get_user()`](#xboxone_get_user), while the "service_config" and "title_id" are the unique ID's for your game on the Xbox Dev Center.

> ### **Parameters**

|    type     | name         | description                                        |
| :---------: | ------------ | -------------------------------------------------- |
| *`pointer`* | **user_id**  | The user ID pointer.                               |
| *`string`*  | **scid**     | The Service Configuration ID (SCID).               |
| *`integer`* | **title_id** | The title ID (as shown in *MicrosoftGame.Config*). |

> ### **Code Sample**
```gml
var xbuser = xboxone_get_user(0);
xboxone_stats_setup(user_id, "00000000-0000-0000-0000-000000000000", 0xFFFFFFFF);
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `xboxone_check_privilege`

> ### **Usage**

`xboxone_check_privilege(user_id, privilege_id, attempt_resolution)`

> ### **Description**

With this function you can check that a given user has a chosen privilege. The function will return true or false in the System Asynchronous Event depending on whether the privilege is enabled or not, and if you set the attempt_resolution argument to true and the privilege isn't enabled, it will also open a system dialogue (suspending the game) to prompt the user to upgrade the account or whatever is required to get the privilege. If the user then enables the required option, the function will return true.

> ### **Parameters**

|     type     | name                   | description                                               |
| :----------: | ---------------------- | --------------------------------------------------------- |
| *`pointer`*  | **user_id**            | The user ID pointer.                                      |
| *`constant`* | **privilege_id**       | The privilege constant to check for `xboxone_privilege_`. |
| *`boolean`*  | **attempt_resolution** | Requests for this privilege.                              |


> ### **Triggers**
> **[Asynchronous System Event]**

|     type     | name           | description                                                                          |
| :----------: | -------------- | ------------------------------------------------------------------------------------ |
|  *`string`*  | **event_type** | Will hold the constant string "check_privilege_result".                              |
| *`integer`*  | **result**     | Will be one or more (in bit-wise combination) of the `xboxone_privilege_` constants. |
| *`constant`* | **privilege**  | The privilege you have requested.                                                    |


> ### **Code Sample**
```gml
var user_one = xboxone_get_activating_user();
xboxone_check_privilege(user_one, xboxone_privilege_multiplayer_sessions, true);
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `xboxone_set_rich_presence`

> ### **Usage**

`xboxone_set_rich_presence(user_id, is_user_active, rich_presence_string)`

> ### **Description**

This function will set the rich presence string for the given user. A Rich Presence string shows a user's in-game activity after the name of the game that the user is playing, separated by a hyphen. This string is displayed under a player's Gamertag in the "Friends & Clubs" list as well as in the player's Xbox Live user profile.

When using this function you need to supply the User ID pointer for the user, and then you can flag the user as currently active in the game or not (using true/false). The next argument is the rich presence string ID to show, and then finally you can (optionally) supply a service_config_id string. Note that this is an optional argument since if you have called [`xboxone_stats_setup()`](#xboxone_stats_setup) you don't need to pass the service_config_id here.

?> **TIP** For more information on rich presence and how to set up the strings to use in the partner center, please see the Microsoft rich presence [documentation](https://docs.microsoft.com/en-us/gaming/xbox-live/features/social/presence/config/live-presence-config2)

> ### **Returns**

|    type     | description                                                  |
| :---------: | ------------------------------------------------------------ |
| *`pointer`* | The user ID pointer.                                         |
| *`string`*  | Flag the user as active or not.                              |
| *`string`*  | The rich presence string ID (defined in the Partner Center). |

> ### **Code Sample**
  
```gml
var userId = xboxone_user_for_pad(0);
xboxone_set_rich_presence(userId, true, "Playing_Challenge");
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `xboxone_fire_event`

> ### **Usage**

`xboxone_fire_event(event_name, ...)`

> ### **Description**

**Description**: This function can be used to fire a stat event. The eventname argument is the name of the event to be fired as defined in the XDP console for your game, and the following additional parameters will also depend on what you have a set up for the stat. The function will return 0 if the event was sent (and should be received/processed by the server) or -1 if there was an error (ie: your event was not setup as the event manifest file included in the project says another number).

> ### **Parameters**

|    type    | name           | description                                                        |
| :--------: | -------------- | ------------------------------------------------------------------ |
| *`string`* | **event_name** | The name of the event to be triggered.                             |
| *`*...`*  | **\<params\>** | The paramenters to be passed to the event as individual arguments. |


> ### **Returns**

|   type   | description                                             |
| :------: | ------------------------------------------------------- |
| *`real`* | -1 on error, 0 if the function was successfully called. |


> ### **Code Sample**
```gml
var xb_user = xboxone_get_user(0);
var uid = xboxone_user_id_for_user(xb_user);
var result = xboxone_fire_event("PlayerSessionStart", uid, global.guid_str, 0, 42, 42);
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `xboxone_read_player_leaderboard`

> ### **Usage**

`xboxone_read_player_leaderboard(ident, player, numitems, friendfilter)`

> ### **Description**

Read a leaderboard starting at a specified user, regardless of the user's rank or score, and ordered by percentile rank. This function requires [`xboxone_stats_setup`](#xboxone_stats_setup) before it can be used.

> ### **Parameters**

|     type     | name             | description                                                                                 |
| :----------: | ---------------- | ------------------------------------------------------------------------------------------- |
|  *`string`*  | **ident**        | The leaderboard id (if filter is `all_players`) or stat to read.                            |
|  *`string`*  | **player**       | The name of the user (returned by [`xboxone_user_id_for_user`](#xboxone_user_id_for_user)). |
|   *`real`*   | **numitems**     | The number of items to retrieve.                                                            |
| *`constant`* | **friendfilter** | One of the `achievement_filter_*` constants.                                         |


> ### **Triggers**
> **[Asynchronous Social Event]**

|     type     | name              | description                                                                     |
| :----------: | ----------------- | ------------------------------------------------------------------------------- |
| *`constant`* | **id**            | Will hold the constant `achievement_leaderboard_info`.                          |
|  *`string`*  | **leaderboardid** | The unique ID for the leaderboard as defined on the provider dashboard.         |
|   *`real`*   | **numentries**    | The number of entries in the leaderboard that you have received.                |
|  *`string`*  | **PlayerN**       | The name of the player, where "N" is position within the received entries list. |
| *`pointer`*  | **PlayeridN**     | The unique user id of the player, "N"                                           |
|   *`real`*   | **RankN**         | The rank of the player "N" within the leaderboard.                              |
|   *`real`*   | **ScoreN**        | The score of the player "N".                                                    |


> ### **Code Sample**
```gml
var xbuser = xboxone_user_for_pad(0);
var tmp = xboxone_user_id_for_user(xbuser);
xboxone_read_player_leaderboard("MyLeaderboard", tmp, 10, achievement_filter_all_players);
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>



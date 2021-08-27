/// @description Insert description here
// You can write your code in this editor

if (async_load[? "id"] != requestId) exit;

if (async_load[? "status"] != true) exit;

var _statName = async_load[? "result"];

var _userId = xboxone_get_user(0);
var _error = xboxone_stats_delete_stat(_userId, _statName);

if (_error == -1) show_debug_message("[ERROR] xboxone_stats_delete_stat");
else show_debug_message("[SUCCESS] xboxone_stats_delete_stat");
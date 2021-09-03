/// @description Insert description here
// You can write your code in this editor


if (async_load[? "id"] != requestId) exit;

if (async_load[? "status"] != true) exit;

var _value = async_load[? "value"];

var _userId = xboxone_get_activating_user();
var _error = xboxone_stats_set_stat_int(_userId, "HighScore", _value);
	
if (_error == -1) show_debug_message("[ERROR] xboxone_stats_set_stat_int");
else show_debug_message("[SUCCESS] xboxone_stats_set_stat_int, new value: " + string(_value));
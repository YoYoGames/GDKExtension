/// @description Insert description here
// You can write your code in this editor

if (async_load[? "id"] != requestId) exit;

if (async_load[? "status"] != true) exit;

var _statName = async_load[? "result"];

var _userId = xboxone_get_user(0);
var _value = xboxone_stats_get_stat(_userId, _statName);
show_message_async(_value);
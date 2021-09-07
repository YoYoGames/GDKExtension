/// @description Insert description here
// You can write your code in this editor

if (async_load[? "id"] != requestId) exit;

if (async_load[? "status"] != true) exit;

var _string = async_load[? "result"];

var _userId = xboxone_get_activating_user();

xboxone_set_rich_presence(_userId, true, _string, SCID);
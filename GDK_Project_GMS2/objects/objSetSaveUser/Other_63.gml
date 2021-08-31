/// @description Insert description here
// You can write your code in this editor

if (async_load[? "id"] != requestId) exit;

if (async_load[? "status"] != true) exit;

var _userIndex = async_load[? "value"];

var _userId = _userIndex;
if (_userId != -1) {
	_userId = xboxone_get_user(_userIndex);
}

var _error = xboxone_set_savedata_user(_userId);
if (_error == -1) show_debug_message("[TODO] xboxone_set_savedata_user");
else show_debug_message("[TODO] xboxone_set_savedata_user");

requestId = noone;
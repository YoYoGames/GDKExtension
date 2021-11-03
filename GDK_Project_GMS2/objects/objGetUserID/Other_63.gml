/// @description Insert description here
// You can write your code in this editor

if (async_load[? "id"] != requestId) exit;

if (async_load[? "status"] != true) exit;

var _index = async_load[? "value"];

// This function will return the userID from a given user index.
// If an invalid index is provided returns 'pointer_null'.
var _userId = xboxone_get_user(_index);
show_message(_userId);
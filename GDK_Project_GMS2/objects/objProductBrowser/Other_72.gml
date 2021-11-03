/// @description DLC loaded

var _id = async_load[? "id"];

// Early exit if we don't need to handle this requestId.
if (dlcLoadRequestId != _id) exit;

show_message(json_encode(async_load));

var _text = buffer_read(dlcLoadBuffer, buffer_string);
buffer_delete(dlcLoadBuffer);
dlcLoadBuffer = noone;
		
var _struct = { loadedData: _text };
show_message(_struct);
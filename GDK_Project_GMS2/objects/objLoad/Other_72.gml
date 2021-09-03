/// @description Insert description here
// You can write your code in this editor

var _id = async_load[? "id"];

for(var i = 0; i < array_length(requests); ++i)
{
	if(requests[i][$ "req_id"] == _id)
	{
		var data = buffer_read(requests[i][$ "buf_id"], buffer_text);
		
		show_debug_message(json_encode(async_load));
		show_debug_message("Loaded '" + data + "' from '" + requests[i][$ "filename"] + "'");
		
		buffer_delete(requests[i][$ "buf_id"]);
		array_delete(requests, i, 1);
		
		break;
	}
}

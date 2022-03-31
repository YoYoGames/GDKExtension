/// @description Insert description here
// You can write your code in this editor
if (async_load[?"event_type"] == "tokenandsignature_result"){
	var status = async_load[?"status"];
	if (status == 0){
		show_message("Token query succeeded. See output.");
		show_debug_message("The token of the user that opened the app: " + async_load[?"token"]);
		if ds_map_exists(async_load, "signature"){			
			show_debug_message("The signature of the http request: " + async_load[?"signature"]);
		}
	}
	else{
		show_message("Token query failed");
	}
}
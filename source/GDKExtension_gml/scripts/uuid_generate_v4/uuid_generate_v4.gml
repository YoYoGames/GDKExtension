function uuid_generate_v4()
{
	randomise();
	
	var guid_bytes = [];
	for(var i = 0; i < 16; ++i)
	{
		array_push(guid_bytes, floor(random(256)));
	}
	
	guid_bytes[@ 6] = (guid_bytes[6] & 0x0F) | 0x40;
	guid_bytes[@ 8] = (guid_bytes[8] & 0x3F) | 0x80;
	
	var guid_str = "";
	var HEX = [ "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F" ];
	
	for(var i = 0; i < 16; ++i)
	{
		var high_nibble = (guid_bytes[i] & 0xF0) >> 4;
		var low_nibble = (guid_bytes[i] & 0x0F);
		
		if(i == 4 || i == 6 || i == 8 || i == 10)
		{
			guid_str += "-";
		}
		
		guid_str += HEX[@high_nibble];
		guid_str += HEX[@low_nibble];
	}
	
	return guid_str;
}

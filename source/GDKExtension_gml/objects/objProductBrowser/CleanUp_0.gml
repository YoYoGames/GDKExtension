/// @description Insert description here
// You can write your code in this editor

// Delete all the cached dynamically created sprites.
var _spriteUris = variable_struct_get_names(spriteLookup);
var _count = array_length(_spriteUris);
repeat(_count) {
	var _uri = _spriteUris[--_count];
	var _spriteId = spriteLookup[$ _uri];
	sprite_delete(_spriteId);
}
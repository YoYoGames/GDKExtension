/// @description Insert description here
// You can write your code in this editor

// Early exit if no button is selected
if (currentIdx == noone || !enabled) exit;

// With selected button
with (buttons[currentIdx]) {

	var _bbTop = bbox_top;
	var _bbLeft = bbox_left;
	var _bbRight = bbox_right;
	var _bbBottom = bbox_bottom;

	// Draw selection box
	draw_set_color(c_white);
	draw_roundrect(_bbLeft - 4, _bbTop - 4, _bbRight + 5, _bbBottom + 5, true);
	draw_roundrect(_bbLeft - 5, _bbTop - 5, _bbRight + 6, _bbBottom + 6, true);
	draw_roundrect(_bbLeft - 6, _bbTop - 6, _bbRight + 7, _bbBottom + 7, true);
}
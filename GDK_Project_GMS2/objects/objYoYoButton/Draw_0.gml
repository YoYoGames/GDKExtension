/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

if (triggerKey == undefined) exit;

draw_set_halign(fa_right);
draw_text(bbox_left - 5, y, "[" + chr(triggerKey) + "]");
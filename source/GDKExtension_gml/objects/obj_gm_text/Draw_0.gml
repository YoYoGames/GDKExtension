
draw_set_valign(v_align)
draw_set_halign(h_align)
draw_set_font(font)
draw_set_color(color)

var spr_width = 4096;		// set to something huge
if(sprite_exists(sprite_index))
{
	draw_self();	
	spr_width = sprite_get_width(sprite_index) * image_xscale;
}

var margin = 32;
var text_width = string_width(text);
if ((text_width > 0) && ((text_width + margin) > spr_width))
{
	var text_scale = (spr_width - margin) / text_width;
	
	draw_text_transformed(x, y, text, text_scale, text_scale, 0);
}
else
{
	draw_text(x,y,text)
}


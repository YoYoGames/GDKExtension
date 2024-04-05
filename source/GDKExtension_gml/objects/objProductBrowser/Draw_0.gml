/// @description Insert description here
// You can write your code in this editor

var user_id = xboxone_get_activating_user();
var pad_index = xboxone_pad_for_user(user_id, 0);

var at_y = y;
var at_x = x;

var _font = fnt_gm_15;
var _descriptionFont = fnt_gm_15;

draw_set_halign(fa_left);
draw_set_valign(fa_top);

/* === Draw tab bar === */

{
	draw_set_font(_font);
	draw_set_colour(c_white);
	
	var tab_text = TAB_NAMES[tab_selected_idx];
	var tab_text_w = string_width(tab_text);
	
	draw_text((at_x + (ITEM_WIDTH / 2) - (tab_text_w / 2)), at_y, tab_text);
}	
	
if (enabled) {
	var sprite_w = sprite_get_width(XboxButtonLB);
	var sprite_h = sprite_get_height(XboxButtonLB);
	
	var scale_x = TAB_BAR_HEIGHT / sprite_w;
	var scale_y = TAB_BAR_HEIGHT / sprite_h;
	
	draw_sprite_ext(XboxButtonLB, -1, at_x, at_y, scale_x, scale_y, 0, c_white, 1);
}
	
if (enabled) {
	var sprite_w = sprite_get_width(XboxButtonRB);
	var sprite_h = sprite_get_height(XboxButtonRB);
	
	var scale_x = TAB_BAR_HEIGHT / sprite_w;
	var scale_y = TAB_BAR_HEIGHT / sprite_h;
	
	draw_sprite_ext(XboxButtonRB, -1, (at_x + ITEM_WIDTH - TAB_BAR_HEIGHT), at_y, scale_x, scale_y, 0, c_white, 1);
}

{
	var sprite_w = sprite_get_width(TabActive);
	var sprite_h = sprite_get_height(TabActive);
	
	var scale_x = TAB_RADIO_SIZE / sprite_w;
	var scale_y = TAB_RADIO_SIZE / sprite_h;
	
	var num_tabs = array_length(TAB_NAMES);
	
	at_x = x + (ITEM_WIDTH / 2) - (TAB_RADIO_SIZE * num_tabs / 2);
	at_y += TAB_BAR_HEIGHT - TAB_RADIO_SIZE;
	
	for(var i = 0; i < num_tabs; ++i)
	{
		var sprite = (i == tab_selected_idx)
			? TabActive
			: TabInactive;
		
		draw_sprite_ext(sprite, -1, at_x, at_y, scale_x, scale_y, 0, c_white, 1);
		
		at_x += TAB_RADIO_SIZE;
	}
}

at_x = x;
at_y += TAB_RADIO_SIZE;

/* === Draw product list === */

if(loading)
{
	draw_set_font(_font);
	draw_set_colour(c_white);
	
	var text = "Loading products...";
	var text_w = string_width(text);
	var text_h = string_height(text);
	
	draw_text(
		(at_x + (ITEM_WIDTH / 2) - (text_w / 2)),
		(at_y + (ITEM_PITCH * MAX_VISIBLE_ITEMS / 2) - (text_h / 2)),
		text);
}

for(var i = product_menu_base_idx; i < array_length(products) && i < product_menu_base_idx + MAX_VISIBLE_ITEMS; ++i)
{
	var product = products[ i];
	
	draw_set_colour(i == product_selected_idx ? c_yellow : c_grey);
	draw_rectangle(x, at_y, x + ITEM_WIDTH, at_y + ITEM_HEIGHT, false);
	
	draw_set_font(_font);
	draw_set_colour(c_black);
	
	var line_height = string_height("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	
	draw_text_ext((x + ITEM_HEIGHT + (2 * PADDING)), (at_y + PADDING),
		product.title,
		line_height, ITEM_WIDTH - ITEM_HEIGHT - (3 * PADDING));
	
	var logo_sprite = product.logo_sprite;
	if(logo_sprite)
	{
		var xs = (ITEM_HEIGHT - (2 * PADDING)) / sprite_get_width(logo_sprite);
		var ys = (ITEM_HEIGHT - (2 * PADDING)) / sprite_get_height(logo_sprite);
		
		draw_sprite_ext(logo_sprite, -1, (x + PADDING), (at_y + PADDING), xs, ys, 0, c_white, 1);
	}
	
	at_y += ITEM_PITCH;
}

/* === Draw stuff under list === */

at_x = x;
at_y = y + TAB_BAR_HEIGHT + (MAX_VISIBLE_ITEMS * ITEM_PITCH);

function draw_sprite_scaled(sprite, pos_x, pos_y, width, height, opacity)
{
	var sprite_w = sprite_get_width(sprite);
	var sprite_h = sprite_get_height(sprite);
	
	var scale_w = width / sprite_w;
	var scale_h = height / sprite_h;
	
	draw_sprite_ext(sprite, -1, pos_x, pos_y, scale_w, scale_h, 0, c_white, opacity);
}

{
	draw_set_font(_font);
	var line_height = string_height("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	
	var opacity, product, product_pkg_id;
	if(product_selected_idx < array_length(products))
	{
		draw_set_colour(c_white);
		opacity = 1.0;
		product = products[product_selected_idx];
		product_pkg_id = storeIdToPackageId[$ product.storeId];
	}
	else{
		draw_set_colour(c_grey);
		opacity = 0.75;
		product = undefined;
		product_pkg_id = undefined;
	}

	if(gamepad_button_check(pad_index, gp_shoulderlb) || keyboard_check(vk_lcontrol))
	{
		draw_sprite_scaled(XboxButtonA, at_x, at_y, line_height, line_height, 1.0);
		draw_text(at_x + line_height, at_y, "Download & Install");
		at_x += ITEM_WIDTH / 2;
		
		if(!is_undefined(product_pkg_id) && variable_struct_exists(mountedPackages, product_pkg_id))
		{
			draw_sprite_scaled(XboxButtonB, at_x, at_y, line_height, line_height, 1.0);
			draw_text(at_x + line_height, at_y, "Unmount");
			at_x -= ITEM_WIDTH / 2;
		}
		else{
			draw_sprite_scaled(XboxButtonB, at_x, at_y, line_height, line_height, 1.0);
			draw_text(at_x + line_height, at_y, "Mount");
			at_x -= ITEM_WIDTH / 2;
		}
	}
	else{
		draw_sprite_scaled(XboxButtonA, at_x, at_y, line_height, line_height, 1.0);
		draw_text(at_x + line_height, at_y, "Purchase");
		at_x += ITEM_WIDTH / 2;
	
		draw_sprite_scaled(XboxButtonB, at_x, at_y, line_height, line_height, 1.0);
		draw_text(at_x + line_height, at_y, "Show product page");
		at_x -= ITEM_WIDTH / 2;
	}
	
	at_y += line_height;
	
	if(product_selected_idx < array_length(products)
		&& (products[product_selected_idx].productKind == e_ms_iap_ProductKind_Consumable || products[product_selected_idx].productKind == e_ms_iap_ProductKind_UnmanagedConsumable))
	{
			draw_sprite_scaled(XboxButtonX, at_x, at_y, line_height, line_height, 1.0);
			draw_text(at_x + line_height, at_y, "Consume (x10)");
			at_x += ITEM_WIDTH / 2;
	
			draw_sprite_scaled(XboxButtonY, at_x, at_y, line_height, line_height, 1.0);
			draw_text(at_x + line_height, at_y, "Check balance");
			at_x -= ITEM_WIDTH / 2;
			at_y += line_height;
	}
	else{
		draw_sprite_scaled(XboxButtonX, at_x, at_y, line_height, line_height, 1.0);
		draw_text(at_x + line_height, at_y, "Acquire license");
		at_x += ITEM_WIDTH / 2;
	
		draw_sprite_scaled(XboxButtonY, at_x, at_y, line_height, line_height, 1.0);
		draw_text(at_x + line_height, at_y, "Preview license");
		at_x -= ITEM_WIDTH / 2;
		at_y += line_height;
	}
	
	draw_sprite_scaled(XboxButtonSelect, at_x, at_y, line_height, line_height, 1.0);
	draw_text(at_x + line_height, at_y, "Refresh");
	at_x += ITEM_WIDTH / 2;
	
	draw_sprite_scaled(XboxButtonLT, at_x, at_y, line_height, line_height, 1.0);
	draw_text(at_x + line_height, at_y, "Alt. functions");
	at_x += ITEM_WIDTH / 2;
}

/* === Draw scroll arrows next to product list === */

at_x = x + ITEM_WIDTH + PADDING;

if(array_length(products) >= MAX_VISIBLE_ITEMS)
{
	draw_set_color(product_menu_base_idx > 0 ? c_white : c_dkgrey);

	var tx = at_x;
	var ty = y + TAB_BAR_HEIGHT;
	
	draw_triangle(
		(tx + (SCROLL_ARROW_SIZE / 2)), ty,
		tx, (ty + SCROLL_ARROW_SIZE),
		(tx + SCROLL_ARROW_SIZE), (ty + SCROLL_ARROW_SIZE),
		false);
	
	draw_set_color((product_menu_base_idx + MAX_VISIBLE_ITEMS) < array_length(products) ? c_white : c_dkgrey);
	
	tx = at_x;
	ty = y + TAB_BAR_HEIGHT + (MAX_VISIBLE_ITEMS * ITEM_PITCH) - PADDING - SCROLL_ARROW_SIZE;
	
	draw_triangle(
		tx, ty,
		(tx + SCROLL_ARROW_SIZE), ty,
		(tx + (SCROLL_ARROW_SIZE / 2)), (ty + SCROLL_ARROW_SIZE),
		false);
}

at_x += SCROLL_ARROW_SIZE + PADDING;

/* === Draw product description and preview image === */

if(product_selected_idx < array_length(products))
{
	var product = products[product_selected_idx];
	
	draw_set_font(_descriptionFont);
	draw_set_colour(c_white);
	
	var line_height = string_height("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	
	var desc_text =
		product.storeId
		+ "\n" + product.description
		+ "\n";
		
	if(product.isInUserCollection)
	{
		desc_text += "\n[isInUserCollection]";
	}
	
	draw_text_ext(at_x, y,
		desc_text,
		line_height, DESCRIPTION_WIDTH);
}

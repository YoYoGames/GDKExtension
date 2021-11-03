/// @description Insert description here
// You can write your code in this editor

enabled = os_type == os_windows;

PADDING = 10;
ITEM_HEIGHT = 120;
ITEM_WIDTH = 500;
ITEM_PITCH = ITEM_HEIGHT + PADDING;
MAX_VISIBLE_ITEMS = 4;
SCROLL_ARROW_SIZE = 20;
DESCRIPTION_WIDTH = 400;

TAB_NAMES = [
	"All",
	"Durables",
	"DLC",
	"Consumables",
	"Bundles",
	"Other",
];

TAB_FILTERS = [
	function(p) { return true; },
	function(p) { return p.productKind == e_ms_iap_ProductKind_Durable && !p.hasDigitalDownload; },
	function(p) { return p.productKind == e_ms_iap_ProductKind_Durable && p.hasDigitalDownload; },
	function(p) { return p.productKind == e_ms_iap_ProductKind_Consumable || p.productKind == e_ms_iap_ProductKind_UnmanagedConsumable; },
	function(p) { return false; /* TODO */ },
	function(p) { return p.productKind != e_ms_iap_ProductKind_Durable && p.productKind != e_ms_iap_ProductKind_Consumable || p.productKind != e_ms_iap_ProductKind_UnmanagedConsumable; },
];

TAB_BAR_HEIGHT = 50;
TAB_RADIO_SIZE = TAB_BAR_HEIGHT / 2;

spriteLookup = {};

allProducts = undefined;
loading = false;

requestId = noone;

dlcLoadRequestId = noone;
dlcLoadBuffer = noone;

products = [];
product_menu_base_idx = 0;
product_selected_idx = 0;
tab_selected_idx = 0;

packageInstallRequests = {};
storeIdToPackageId = {};
mountedPackages = {};

enumPackagesRequest = ms_iap_EnumeratePackages(e_ms_iap_PackageKind_Content, e_ms_iap_PackageEnumerationScope_ThisOnly);

function reload_products()
{
	// Get the user ID pointer for the current active user.
	var _userId = xboxone_get_activating_user();
	
	// The constants e_ms_iap_ProductKind_* can be combined in a bitwise OR
	// operation to query for multiple product types within the same query call.
	var _queryTypes = e_ms_iap_ProductKind_Consumable
		| e_ms_iap_ProductKind_Durable
		| e_ms_iap_ProductKind_Game
		| e_ms_iap_ProductKind_Pass
		| e_ms_iap_ProductKind_UnmanagedConsumable;
	
	// We can query products using the following functiom call it will
	// generate and async request and return its id if the id is negative then
	// there was an error while issuing the query.
	requestId = ms_iap_QueryAssociatedProducts(_userId, _queryTypes);
	if(requestId < 0)
	{
		show_debug_message("ms_iap_QueryAssociatedProducts failed (" + string(requestId) + ")");
		return;
	}
	
	// We are now loading set the flag to true.
	loading = true;
	
	// Reset all query related variables.
	allProducts = undefined;
	products = [];
	product_menu_base_idx = 0;
	product_selected_idx = 0;
}

function repopulate_products()
{
	// Reset all query related variables.
	products = [];
	product_menu_base_idx = 0;
	product_selected_idx = 0;
	
	var filter_func = TAB_FILTERS[tab_selected_idx];
	
	for(var i = 0; i < array_length(allProducts); ++i)
	{
		var product = allProducts[i];
		
		if(filter_func(product))
		{
			array_push(products, product);
		}
	}
	
	for(var p = 0; p < array_length(products); ++p)
	{
		var product = products[p];
		
		for(var i = 0; i < array_length(product[$"images"]); ++i)
		{
			var image = product[$ "images"][i];
			if (image[$ "imagePurposeTag"] == "Logo")
			{
				var _uri = image[$"uri"];
				
				// Create a dynamically sprite and cache it for later.
				if (!variable_struct_exists(spriteLookup, _uri)) {
					var _spriteId = sprite_add(image[$"uri"], 1, false, false, 0, 0);
					spriteLookup[$ _uri] = _spriteId;
				}
				
				// Read product logo sprite from lookup cache.
				product[$ "logo_sprite"] = spriteLookup[$ _uri];
			}
		}
	}
}


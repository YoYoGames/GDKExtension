/// @description Insert description here
// You can write your code in this editor

if(is_undefined(allProducts) && !loading)
{
	reload_products();
	exit;
}

var _userId = xboxone_get_activating_user();
var _padIndex = xboxone_pad_for_user(_userId, 0);

// Use right shoulder trigger to toogle navigation on/off.
if (gamepad_button_check_pressed(_padIndex, gp_shoulderrb) && os_type != os_windows) {
	enabled = !enabled;
}
if (!enabled) return;

if(gamepad_button_check_pressed(_padIndex, gp_shoulderl) || keyboard_check_pressed(vk_left))
{
	if(tab_selected_idx > 0)
	{
		--tab_selected_idx;
		repopulate_products();
	}
}

if(gamepad_button_check_pressed(_padIndex, gp_shoulderr) || keyboard_check_pressed(vk_right))
{
	if((tab_selected_idx + 1) < array_length(TAB_NAMES))
	{
		++tab_selected_idx;
		repopulate_products();
	}
}

if(gamepad_button_check_pressed(_padIndex, gp_padu) || keyboard_check_pressed(vk_up))
{
	if(product_selected_idx > 0)
	{
		--product_selected_idx;
	
		if(product_menu_base_idx > 0 && product_selected_idx == product_menu_base_idx)
		{
			--product_menu_base_idx;
		}
	}
}

if(gamepad_button_check_pressed(_padIndex, gp_padd) || keyboard_check_pressed(vk_down))
{
	if((product_selected_idx + 1) < array_length(products))
	{
		++product_selected_idx;
		
		if ((product_selected_idx - product_menu_base_idx) > (MAX_VISIBLE_ITEMS - 2)) {
			product_menu_base_idx = min(product_menu_base_idx + 1, array_length(products) - MAX_VISIBLE_ITEMS);
		}
	}
}

if(gamepad_button_check_pressed(_padIndex, gp_select) || keyboard_check_pressed(vk_space))
{
	reload_products();
}

if(gamepad_button_check_pressed(_padIndex, gp_face1) || keyboard_check_pressed(ord("X")))
{
	// If product array is not empty
	if(array_length(products) != 0)
	{
		var _product = products[product_selected_idx];
		var _storeId = _product.storeId;
		
		if(gamepad_button_check(_padIndex, gp_shoulderlb) || keyboard_check(vk_lcontrol))
		{
			// Make a downdload and install request (returns an Async IAP Event id)
			var _requestId = ms_iap_DownloadAndInstallPackages(_userId, [ _storeId ]);
			if(_requestId > 0)
			{
				packageInstallRequests[$ _requestId] = _storeId;
			}
		}
		else {
			ms_iap_ShowPurchaseUI(_userId, _storeId, undefined, undefined);
		}
	}
}

if(gamepad_button_check_pressed(_padIndex, gp_face2) || keyboard_check_pressed(ord("Z")))
{
	// If product array is not empty
	if(array_length(products) != 0)
	{
		var _product = products[product_selected_idx];
		var _storeId = _product.storeId;
		
		if(gamepad_button_check(_padIndex, gp_shoulderlb) || keyboard_check(vk_lcontrol))
		{
			// Early exit if packageId doesn't exist
			if(variable_struct_exists(storeIdToPackageId, _storeId)) {
			
				var _packageId = storeIdToPackageId[$ _storeId];
				var _package = mountedPackages[$ _packageId];
				
				// Check if it exists (not undefined)
				if (!is_undefined(_package)) {
					ms_iap_UnmountPackage(_packageId);
				}
				else {
					ms_iap_MountPackage(_packageId);
				}
			}
		}
		else{
			ms_iap_ShowProductPageUI(_userId, _storeId);
		}
	}
}

if(gamepad_button_check_pressed(_padIndex, gp_face3) || keyboard_check_pressed(ord("A")))
{
	// If product array is not empty
	if (array_length(products) != 0) {
		
		// Get current selected product
		var _product = products[product_selected_idx];
	
		var _productKind = _product.productKind;
		var _storeId = _product.storeId;  
		
		// Check if _productKind is either Consumable or UnmanagedConsumable
		if(_productKind & (e_ms_iap_ProductKind_Consumable | e_ms_iap_ProductKind_UnmanagedConsumable)) {
			
			// Need a unique tracking_id each time, or else the XStore call
			// will succeed, but have no effect.
			var _guid = uuid_generate_v4(); 
			ms_iap_ReportConsumableFulfillment(_userId, _storeId, 10, _guid);
		}
		// Check if this product is a package
		else if(variable_struct_exists(storeIdToPackageId, _storeId)) {
			
			ms_iap_AcquireLicenseForPackage(_userId, storeIdToPackageId[$ _storeId]);
		}
		// It's neither a consumable/package
		else {
			
			ms_iap_AcquireLicenseForDurables(_userId, _storeId);
		}
	}
}

if(gamepad_button_check_pressed(_padIndex, gp_face4) || keyboard_check_pressed(ord("S")))
{
	// Early exit if there are no products
	if (array_length(products) != 0) {

		var _product = products[product_selected_idx];
		
		var _productKind = _product.productKind;
		var _storeId = _product.storeId;  
		
		// Check if _productKind is either Consumable or UnmanagedConsumable
		if(_productKind & (e_ms_iap_ProductKind_Consumable | e_ms_iap_ProductKind_UnmanagedConsumable)) {
			
			ms_iap_QueryConsumableBalanceRemaining(_userId, _storeId);
		}
		// Check if this product is a package
		else if(variable_struct_exists(storeIdToPackageId, _storeId)) {
			
			ms_iap_CanAcquireLicenseForPackage(_userId, storeIdToPackageId[$ _storeId]);
		}
		// It's neither a consumable/package
		else {
			
			ms_iap_CanAcquireLicenseForStoreId(_userId,_storeId);
		}
	}
}

/*
if(gamepad_button_check_pressed(_padIndex, gp_shoulderlb))
{
	ms_iap_QueryGameLicense(_userId);
}

if(gamepad_button_check_pressed(_padIndex, gp_shoulderrb))
{
	ms_iap_QueryAddOnLicenses(_userId);
}
*/
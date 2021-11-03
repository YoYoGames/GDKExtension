/// @description Insert description here
// You can write your code in this editor

show_debug_message("Got IAP event!");
show_debug_message(json_encode(async_load));

switch (async_load[?"type"]) {

	case "ms_iap_QueryAssociatedProducts_result":
	
		if (async_load[?"id"] != requestId) exit;
		if(!async_load[? "status"]) break;

		allProducts = async_load[?"results"];
		repopulate_products();

		loading = false;
		break;
		
	case "ms_iap_DownloadAndInstallPackages_result":
	
		var _requestId = async_load[?"id"];
	
		// Early exit there is no request for specified id.
		if(!variable_struct_exists(packageInstallRequests, _requestId)) exit;
		
		// If status is not success break.
		if(!async_load[? "status"]) break;
		
		var _storeId = packageInstallRequests[$ _requestId];
		
		// We read position 0 since we know we just have one request.
		var _packageId = async_load[? "package_ids"][0];
		
		// Create a storeId to packageId mapping.
		storeIdToPackageId[$ _storeId] = _packageId;
		
		variable_struct_remove(packageInstallRequests, _requestId);
		break;
		
	case "ms_iap_MountPackage_result":
	
		// If status is not success break.
		if(!async_load[? "status"]) break;
		
		var _packageId = async_load[? "package_id"];
		var _mountPath = async_load[? "mount_path"];
		
		mountedPackages[$ _packageId] = _mountPath;
		show_message(json_encode(async_load));
		
		// Now we will load the contents of the DLC:
		// For the purposes of this demo example the DLC contains a single file "sampleDLC.txt"
		dlcLoadBuffer = buffer_create(1, buffer_grow, 1);
		switch (os_type) {
			// Both systems use the buffer_load_async sync the DLC is mounted on local filesystem
			case os_xboxseriesxs: case os_windows:
				dlcLoadRequestId = buffer_load_async(dlcLoadBuffer, _mountPath + "/sampleDLC.txt", 0, -1);
				break;
			default: throw "[ERROR] objProductBrowser, unsupported platform";
		}
		
		break;
		
	case "ms_iap_UnmountPackage_result":

		var _packageId = async_load[? "package_id"];
		
		variable_struct_remove(mountedPackages, _packageId);
		
		show_message(json_encode(async_load));
		break;
		
	case "ms_iap_EnumeratePackages_result":
		
		// Early exit if request id doesn't match.
		if (async_load[?"id"] != enumPackagesRequest) exit;
		
		// Early exit if status fails.
		if(!async_load[?"status"]) break;
		
		// Loop through packages and addd them to local lookup struct.
		var _packages = async_load[? "results"];
		for(var _i = 0; _i < array_length(_packages); ++_i)
		{
			var _package = _packages[_i];
			
			var package_id = _package.packageIdentifier;
			var store_id = _package.storeId;
			
			storeIdToPackageId[$ store_id] = package_id;
		}
		
		// Reset request id
		enumPackagesRequest = noone;
		break;
	
	// General methods callback event
	case "ms_iap_AcquireLicenseForDurables_result":
	case "ms_iap_AcquireLicenseForPackage_result":
	case "ms_iap_CanAcquireLicenseForPackage_result":
	case "ms_iap_CanAcquireLicenseForStoreId_result":
	case "ms_iap_ReportConsumableFulfillment_result":
	case "ms_iap_QueryConsumableBalanceRemaining_result":
	case "ms_iap_QueryGameLicense_result":
		var _jsonString = json_encode(async_load);
		show_message(_jsonString)
}

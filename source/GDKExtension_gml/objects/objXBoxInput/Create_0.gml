/// @description Initialize variables

// This input manager should only exist when
// running on the Xbox Series X/S target.
if (os_type != os_xboxseriesxs) {
	instance_destroy();
	exit;
}

enabled = true;
canDisabled = false;

buttons = []; // An array with all the avaiable buttons
currentIdx = noone; // The currently selected button (noone, otherwise)


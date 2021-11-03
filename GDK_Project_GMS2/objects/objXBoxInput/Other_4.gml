/// @description Insert description here
// You can write your code in this editor

// This will manage the XBoxInput control
// some rooms require you to toogle it ON or OFF
// this can be done here!
switch (room) {
	case rmIAP:
		canDisabled = true;
		break;
	default: 
		canDisabled = false;
		enabled = true;
		break;
}

var _buttons = buttons;

// Add all the buttons to the buttons array.
with (objXBoxButton) {
	array_push(_buttons, id);
}

// Early exit if there are no buttons.
if (array_length(_buttons) == 0) exit;

// Sort the buttons array by 'tabPriority'
// This property is set under each button creation code
// and refers to the order in which it gets selected
array_sort(_buttons, function(_b1, _b2) {
	return _b1.tabPriority - _b2.tabPriority;
});

// Set currently selected button index to 0 and flag the button
// as selected to it's rendered properly.
currentIdx = 0
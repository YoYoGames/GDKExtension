/// @description Insert description here
// You can write your code in this editor

// Early exit if there are no buttons.
if (array_length(buttons) == 0) exit;

// Get activating user pad index
// The active user can change during game it's up to the developer
// to keep track of which gamepad to check for.
var _userId = xboxone_get_activating_user();
var _padIndex = xboxone_pad_for_user(_userId, 0);

var _newIndex = currentIdx;

// Use right shoulder trigger to toogle navigation on/off.
if (gamepad_button_check_pressed(_padIndex, gp_shoulderrb) && canDisabled) {
	enabled = !enabled;
}
if (!enabled) return;

// Use left/right shoulder buttons to switch between buttons.
if (gamepad_button_check_pressed(_padIndex, gp_shoulderl)) {
	_newIndex--;
}
else if (gamepad_button_check_pressed(_padIndex, gp_shoulderr)) {
	_newIndex++;
}

// If we moved to a new button index
if (_newIndex != currentIdx) {

	// Warp the button index around
	if (_newIndex < 0) _newIndex = array_length(buttons) - 1;
	else if (_newIndex >= array_length(buttons)) _newIndex = 0;

	// Set new currently selected index
	currentIdx = _newIndex;
}

// If the "A" button is pressed in the controller
// trigger current buttons 'onClick' method (if not locked)
if (gamepad_button_check_pressed(_padIndex, gp_face1)) {
	
	if (buttons[currentIdx].locked) exit;
	
	buttons[currentIdx].onClick();
}
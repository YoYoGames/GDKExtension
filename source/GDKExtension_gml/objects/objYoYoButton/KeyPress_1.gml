/// @description Redirect to mouse

if (triggerKey != undefined) {
	if (keyboard_check_pressed(triggerKey)) {
		event_perform(ev_mouse, ev_left_release);
	}
}
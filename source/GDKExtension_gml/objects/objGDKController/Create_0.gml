
// This controller is resposible for: initializing; updating and quitting the GDK extension
// and it is not required unless we are on the windows platform.
if (os_type != os_windows) {
	instance_destroy();
	return;
}

#macro SCID "00000000-0000-0000-0000-00006a2a0983"

show_debug_message("##### Initialising GDK #####");

// Must be called before any other GDK extension function, recommend using a
// Controller persistent object that is created or placed in the first room
// and this call is in the create event.
//
// This function must be passed in the SCID present in the MicrosoftGame.config file.
gdk_init(SCID);

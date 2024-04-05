
// This controller is resposible for: initializing; updating and quitting the GDK extension
// and it is not required unless we are on the windows platform.
if (os_type != os_windows) {
	instance_destroy();
	return;
}

// This function is required to be called before using any other GDK Extension functions,
// failing to do so will result in errors to every extension call.
// If auto initialise is enabled you are not required to use this function.
//
// This function must be passed in the SCID present in the MicrosoftGame.config file.
//gdk_init(SCID);

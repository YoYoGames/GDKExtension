
show_debug_message("##### Quitting GDK #####");

// This should be called on close down and no GDK extension functions should be called
// after it, recommend using a Controller persistent object that is created or placed
// in the first room and this call is in the destroy event.
gdk_quit();

show_debug_message("[INFO] GDK quitted successfully!");
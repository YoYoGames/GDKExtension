show_debug_message( "##### Calling gdk_init()" );
gdk_init();
show_debug_message( "##### Finished gdk_init()" );

xboxone_show_account_picker( 0, 0 );

// gdk_update();

// * Documentation (README.md)
// * Add params to gdk_init for service parameters -
// * config file.... Title ID etc.... Do we need this???
// * MS Store packaging .... we will need a HOWTO 
// * Connected Storage .... we will need something to bring over (probably need a buffer_save for connected storage)

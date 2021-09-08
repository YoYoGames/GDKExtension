
## Initial Configuration

Creating GDK enabled projects requires the developer to pack a *Config* file with the game (as an included file). The required file is called **MicrosoftGame.Config** and can be created/edited with a GUI Tool provided by Microsoft GDK installation.

?> **INFO** The following link [MicrosoftGame.config Editor](https://docs.microsoft.com/en-us/gaming/gdk/_content/gc/system/overviews/microsoft-game-config/microsoftgameconfig-editor) provides full documentation on how to create and edit your *MicrosoftGame.Config* file.

## Event-based Statistics

To use event-base statistics (2013 stats system) you must first have set it up correctly in the Service Configuration page for your game on the Xbox Developer Portal. You can find further information on this from the [Microsoft Developer Site](https://developer.xboxlive.com/en-us/platform/development/documentation/xdp/Pages/atoc_xdp_selfservice_xdpdocs.aspx).

Once you've configured a list of stats on XDP, click “Publish and Download” on the Stats page. This should prompt you to download a manifest file. This file then needs to be added to GameMaker: Studio as an included file and is automatically loaded by the extension during initialization.
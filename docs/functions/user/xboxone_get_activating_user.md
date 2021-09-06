## `xboxone_get_activating_user()`

> ### **Description**

With this function you can retrieve the user ID pointer for the user that launched the game from the console. Note that this is rarely what you want to do in a game, because this user can logout while the game is running.

> ### **Returns**

|    type     | description          |
| :---------: | -------------------- |
| *`pointer`* | The user ID pointer. |

> ### **Code Sample**
  
```gml
global.main_user = xboxone_get_activating_user();
```



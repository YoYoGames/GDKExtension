
## `xboxone_show_account_picker` <span class="badge badge-danger">TODO</span>

> ### **Usage**

`xboxone_show_account_picker(arg0, arg1)`

> ### **Description**

This function launches the system’s account picker which will associate the selected user with the specified pad. The "mode" argument is either 0 or 1 – if 0 is specified no guest accounts can be selected, while 1 allows guest accounts. This function returns an asynchronous event ID value, and when the account picker closes an asynchronous dialog event will be triggered with details of the result of the operation.

> ### **Parameters**

|    type     | name       | description                    |
| :---------: | ---------- | ------------------------------ |
| *`string`*  | **param1** | This is an important parameter |
| *`pointer`* | **param1** | This is an optional paramenter |



> ### **Triggers**
> **[Async Dialog Event]**

|    type     | name       | description                    |
| :---------: | ---------- | ------------------------------ |
| *`string`*  | **param1** | This is an important parameter |
| *`pointer`* | **param1** | This is an optional paramenter |



> ### **Returns**

|    type    | description                    |
| :--------: | ------------------------------ |
| *`string`* | This is an important parameter |



> ### **Code Sample**
  
```gml
show_debug_message("Hello world!");
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `xboxone_get_activating_user`

> ### **Usage**

`xboxone_get_activating_user()`

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
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `xboxone_get_user_count`

> ### **Usage**

`xboxone_get_user_count()`

> ### **Description**

With this function you can find the total number of users currently signed in to the system. The returned value will be an integer value.

> ### **Returns**

|   type   | description                                                  |
| :------: | ------------------------------------------------------------ |
| *`real`* | The total number of users currently signed in to the system. |

> ### **Code Sample**
  
```gml
for (var i = 0; i < xboxone_get_user_count(); i++) {
    user_id[i] = xboxone_get_user(i);
}
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `xboxone_get_user`

> ### **Usage**

`xboxone_get_user(index)`

> ### **Description**

Description With this function you can retrieve the user ID pointer for the indexed user. If the user does not exist, the function will return the constant pointer_null instead. You can find the number of users currently logged in with the function [`xboxone_get_user_count()`](#xboxone_get_user_count).


> ### **Parameters**

|    type     | name      | description                        |
| :---------: | --------- | ---------------------------------- |
| *`integer`* | **index** | The index to get the User ID from. |


> ### **Returns**

|    type     | description          |
| :---------: | -------------------- |
| *`pointer`* | The user ID pointer. |


> ### **Code Sample**
  
```gml
for (var i = 0; i < xboxone_get_user_count(); i++) {
    user_id[i] = xboxone_get_user(i);
}
```
<div style="page-break-after: always;"></div>

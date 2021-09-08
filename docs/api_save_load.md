## `gdk_save_group_begin`

> ### **Usage**

`gdk_save_group_begin(container_name)`

> ### **Description**

This function is called when you want to begin the saving out of multiple buffers to multiple files. The "container_name" is a string and will be used as the directory name for where the files will be saved, and should be used as part of the file path when loading the files back into the IDE later (using any of the buffer_load() functions). This function is only for use with the [`gdk_save_buffer()`](#gdk_save_buffer) function and you must also finish the save definition by calling [`gdk_save_group_end()`](#gdk_save_group_end) function otherwise the files will not be saved out.

> ### **Parameters**

|    type    | name               | description                |
| :--------: | ------------------ | -------------------------- |
| *`string`* | **container_name** | The name of the container. |


> ### **Code Sample**
```gml
gdk_save_group_begin("SaveGame");
save1 = gdk_save_buffer(buff1, "Player_Save1.sav", 0, 16384);
save2 = gdk_save_buffer(buff2, "Player_Save2.sav", 0, 16384);
save3 = gdk_save_buffer(buff3, "Player_Save3.sav", 0, 16384);
save4 = gdk_save_buffer(buff4, "Player_Save4.sav", 0, 16384);
gdk_save_group_end();
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `gdk_save_buffer`

> ### **Usage**

`gdk_save_buffer(buffer_idx, filename, offset, size)`

> ### **Description**

With this function you can save part of the contents of a buffer to a file, ready to be read back into memory using the [`gdk_save_buffer()`](#gdk_save_buffer) function (or any of the other functions for loading buffers). The "offset" defines the start position within the buffer for saving (in bytes), and the "size" is the size of the buffer area to be saved from that offset onwards (also in bytes).

This function works asynchronously, and so the game will continue running while being saved, and all files saved using this function will be placed in a *"default"* folder. This folder does not need to be included in the filename as it is added automatically by GameMaker. For example the filename path `"Data\Player_Save.sav"` would actually be saved to `"default\Data\Player_Save.sav"`. However, if you then load the file using the function [`gdk_load_buffer()`](#gdk_load_buffer), you do not need to supply the "default" part of the path either.

> ### **Parameters**

|    type     | name           | description                                                                 |
| :---------: | -------------- | --------------------------------------------------------------------------- |
| *`integer`* | **buffer_idx** | The index of the buffer to save.                                            |
| *`string`*  | **filename**   | The place where to save the buffer to (path + filename + extension).        |
| *`integer`* | **offset**     | The start position within the buffer for saving (in bytes).                 |
| *`integer`* | **size**       | The size of the buffer area to be saved from the offset onwards (in bytes). |


> ### **Returns**

|   type   | description                             |
| :------: | --------------------------------------- |
| *`real`* | -1 on error, any other value otherwise. |


> ### **Triggers**
> **[Asynchronous Save/Load Event]**

|    type     | name       | description                                                                |
| :---------: | ---------- | -------------------------------------------------------------------------- |
| *`integer`* | **id**     | Will hold the unique identifier of the asynchronous request.               |
|  *`real`*   | **error**  | 0 if successful, some other value if there has been an error (error code). |
|  *`real`*   | **status** | 1 if successful, 0 if failed.                                              |

> ### **Code Sample**
```gml
	saveid = gdk_save_buffer(buff, "Player_Save.sav", 0, 16384);
```
The code above will initiate a asynchronous buffer save operation that will trigger an asynchronous Save/Load event where we can check if the task was successful or not, with the following code:
```gml
	if (async_load[? "id"] == saveid) {
    	if (async_load[? "status"]== false) {
        	show_debug_message("Save failed!");
        }
		else {
			show_debug_message("Save succeeded!");
		}
    }
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `gdk_save_group_end`

> ### **Usage**

`gdk_save_group_end()`

> ### **Description**

This function finishes the definition of a buffer save group. You must have previously called the function [`gdk_save_group_begin()`](#gdk_save_group_begin) to initiate the group, then call the function [`gdk_save_buffer()`](#gdk_save_buffer) for each file that you wish to save out. Finally you call this function, which will start the saving of the files. The function will return a unique ID value for the save, which can then be used in the Asynchronous Save / Load event to parse the results from the async_load DS map.

> ### **Triggers**
> **[Asynchronous Save/Load Event]**

|    type     | name       | description                                                                |
| :---------: | ---------- | -------------------------------------------------------------------------- |
| *`integer`* | **id**     | Will hold the unique identifier of the asynchronous request.               |
|  *`real`*   | **error**  | 0 if successful, some other value if there has been an error (error code). |
|  *`real`*   | **status** | 1 if successful, 0 if failed.                                              |


> ### **Code Sample**
```gml
buffer_async_group_begin("SaveGame");
buffer_save_async(buff1, "Player_Save1.sav", 0, 16384);
buffer_save_async(buff2, "Player_Save2.sav", 0, 16384);
buffer_save_async(buff3, "Player_Save3.sav", 0, 16384);
buffer_save_async(buff4, "Player_Save4.sav", 0, 16384);
saveGroup = buffer_async_group_end();
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `xboxone_set_savedata_user`

> ### **Usage**

`xboxone_set_savedata_user(user_id)`

> ### **Description**

This function specifies that future file operations which operate in the save game area (i.e. all file writes, and reads from files that aren't in the bundle area) will be associated with the specified user. This can be called as often as necessary to redirect save data to the appropriate user, or you can use the constant `pointer_null` to lock save/load features.

> ### **Parameters**

|    type     | name        | description          |
| :---------: | ----------- | -------------------- |
| *`pointer`* | **user_id** | The user ID pointer. |


> ### **Code Sample**
```gml
if xboxone_get_savedata_user() != user_id[0] {
    xboxone_set_savedata_user(user_id[0]);
}
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `xboxone_get_savedata_user`

> ### **Usage**

`xboxone_get_savedata_user()`

> ### **Description**

This function returns the user ID pointer (or the constant `pointer_null`) currently associated with file saving. See [`xboxone_set_savedata_user()`](#xboxone_set_savedata_user) for further details.

> ### **Returns**

|    type     | description                                     |
| :---------: | ----------------------------------------------- |
| *`pointer`* | The user ID currently being used for save data. |


> ### **Code Sample**
```gml
if xboxone_get_savedata_user() != user_id[0] {
    xboxone_set_savedata_user(user_id[0]);
}
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>


## `gdk_load_buffer`

> ### **Usage**

`gdk_load_buffer(buffer_idx, filename, offset, size)`

> ### **Description**

With this function you can load a file that you have created previously using the [`gdk_save_buffer()`](#gdk_save_buffer) function into a buffer. The "offset" defines the start position within the buffer for loading (in bytes), and the "size" is the size of the buffer area to be loaded from that offset onwards (also in bytes). You can supply a value of -1 for the size argument and the entire buffer will be loaded. Note that the function will load from a "default" folder, which does not need to be included as part of the file path you provide. This folder will be created if it doesn't exist or when you save a file using [`gdk_save_buffer()`](#gdk_save_buffer).

> ### **Parameters**

|    type     | name           | description                                         |
| :---------: | -------------- | --------------------------------------------------- |
| *`integer`* | **buffer_idx** | The index of the buffer to load.                    |
| *`string`*  | **filename**   | The name of the file to load.                       |
| *`integer`* | **offset**     | The offset within the buffer to load to (in bytes). |
| *`integer`* | **size**       | The size of the buffer area to load (in bytes).     |

> ### **Returns**

|   type   | description                                                             |
| :------: | ----------------------------------------------------------------------- |
| *`real`* | -1 if there was an error, otherwise the id of the asynchronous request. |


> ### **Triggers**
> **[Asynchronous Save/Load Event]**

|    type     | name          | description                                                                |
| :---------: | ------------- | -------------------------------------------------------------------------- |
| *`integer`* | **id**        | Will hold the unique identifier of the asynchronous request.               |
|  *`real`*   | **error**     | 0 if successful, some other value if there has been an error (error code). |
|  *`real`*   | **status**    | 1 if successful, 0 if failed.                                              |
|  *`real`*   | **file_size** | The total size of the file being loaded (in bytes).                        |
|  *`real`*   | **load_size** | The amount of bytes loaded into the buffer.                                |

> ### **Code Sample**
```gml
loadid = gdk_load_buffer(buff, "Player_Save.sav", 0, 16384);
```
then in the asynchronous Save/Load event we can check if the task was successful or not, like this:
```gml
if (async_load[? "id"] == saveid) {
    if (async_load[? "status"]== false) {
        show_debug_message("Load failed!");
    }
    else {
        show_debug_message("Load succeeded!");
    }
}
```
<hr class="delimiter">
<div style="page-break-after: always;"></div>

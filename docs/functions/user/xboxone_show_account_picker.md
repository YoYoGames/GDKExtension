
## `xboxone_show_account_picker(arg0, arg1)` <span class="badge badge-danger">TODO</span>

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

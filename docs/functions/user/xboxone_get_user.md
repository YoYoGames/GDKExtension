## `xboxone_get_user(index)`

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

## `xboxone_get_user_count()`

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
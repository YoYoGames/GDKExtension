## `gdk_init(scid)` <span class="badge badge-primary">REQUIRED</span>

> ### **Description**

Must be called before any other GDK extension function, recommend using a Controller persistent object that is created or placed in the first room and this call is in the create event.

> ### **Parameters**

|    type    | name     | description                   |
| :--------: | -------- | ----------------------------- |
| *`string`* | **scid** | The Service Configuration ID. |

> ### **Code Sample**
  
```gml
gdk_init("00000000-0000-0000-0000-000060ddb039");
```

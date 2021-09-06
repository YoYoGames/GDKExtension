## `gdk_init` <span class="badge badge-primary">REQUIRED</span>

> ### **Usage**

`gdk_init(scid)`

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
<div style="page-break-after: always;"></div>


## `gdk_update` <span class="badge badge-primary">REQUIRED</span>

> ### **Usage**

`gdk_update()`

> ### **Description**

This should be called each frame while GDK extension is active, recommend using a Controller persistent object that is created or placed in the first room and this call is in the step event.

> ### **Code Sample**
  
```gml
gdk_update();
```
<div style="page-break-after: always;"></div>


## `gdk_quit` <span class="badge badge-primary">REQUIRED</span>

> ### **Usage**

`gdk_quit()`

> ### **Description**

This should be called on close down and no GDK extension functions should be called after it, recommend using a Controller persistent object that is created or placed in the first room and this call is in the destroy event.

> ### **Code Sample**
  
```gml
gdk_quit();
```
<div style="page-break-after: always;"></div>
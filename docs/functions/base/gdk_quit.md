## `gdk_quit()` <span class="badge badge-primary">REQUIRED</span>

> ### **Description**

This should be called on close down and no GDK extension functions should be called after it, recommend using a Controller persistent object that is created or placed in the first room and this call is in the destroy event.

> ### **Code Sample**
  
```gml
gdk_quit();
```
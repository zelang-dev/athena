# Athena

Build a *minimal* native `GUI` *desktop* **Linux** `APP` with ease.

This project is forked and remodeled from <https://github.com/UlricE/Mowitz>, it's aim is to quickly create native **Linux** desktop **APP** without a major/large dependency tree.

## This started out coming from [c-gui](https://github.com/zelang-dev/c-gui) for better maintenance

* With no prior understanding of X11 programming inferface, mainly inspired by following the pattern layout in [Minimal cross-platform graphics](https://zserge.com/posts/fenster). Digging more into *X11 universe*, the aesthetics part seemed to be a big after thought. Plenty of info on **X11 API**, but no official code tutorials. The closet that has actually example code is [X-windows: programming with Athena widgets](https://ergodic.ugr.es/cphys_pedro/unix/intro.html).
* Began with a simplier menu layout derived from [X11 Menus (how to)](https://www.linuxquestions.org/questions/programming-9/x11-menus-how-to-839904/), the source for `OpenGL` dependency.
* Switched to an alterative [Athena Widgets/Xaw implementations](https://forums.freebsd.org/threads/athena-widgets-xaw-implementations.81588/) toolkit, parts of this *library* includes various aspects of [Survey of Widget Sets](http://www.efalk.org/Widgets/).
* To my discovery there is a project [Mowitz](https://github.com/UlricE/Mowitz) that has all basic **GUI** *aesthetic* features for Linux covered. This includes a native [WebView](https://en.wikipedia.org/wiki/WebView) widget implementation. It's [Kylie](https://siag.nu/kylie/), *very buggy, leaks, only **http**,* but is a starting point for **NO GTK** needed, **PR's are welcome**. More work is required to bring it up to standards. Currently, the **kylie** *application* has been striped back down to a *reusable* widget. Adding [opentls](https://github.com/zelang-dev/opentls) for secure connection handling, [stb_image.h](https://github.com/nothings/stb), [nanosvg.h](https://github.com/memononen/nanosvg), and [libtiff](https://libtiff.gitlab.io/libtiff/) for image handling.
* This project embeds a modified version of the [webview-c](https://github.com/javalikescript/webview-c). The examples has been tested working as expected on **Windows** and **macOS**.

## Installation

[CMake](https://cmake.org) `FetchContent` and `find_package` is use here to setup your project `App`.

You **WILL** need to *`#define`* `ATHENA_ICON`, `ATHENA_COLOR` to set *window color* and *window icon*

```sh
find_package(athena QUIET CONFIG)
if(NOT athena_FOUND)
    FetchContent_Declare(athena
        URL https://github.com/zelang-dev/athena/archive/refs/heads/main.zip
        #URL https://github.com/zelang-dev/athena/archive/refs/tags/v0.5.0.zip
        #URL_MD5 6e9756dee0ef5903d850dc021d5df725
    )
    FetchContent_MakeAvailable(athena)
endif()
target_include_directories(your_project
 PRIVATE $<BUILD_INTERFACE:${ATHENA_INCLUDE_DIR} $<INSTALL_INTERFACE:${ATHENA_INCLUDE_DIR})
target_link_libraries(your_project PUBLIC ATHENA::WIDGETS)
```

### Usage

```c
#include <Athena.h>

#define IDC_FIELD1 10
#define IDC_FIELD2  20
#define IDC_FIELD3 30
#define IDC_FIELD4 40

void form_prompt(__ATS_MENU__) {
 ats_t ui = {0};
 ui_field form[] = {
   {IDC_FIELD1, field_text, "Name", "Free alternative to the Motif XmTextField", 290, 40, 1},
   {IDC_FIELD2, field_secret, "Password", "Fixed Length", 130, 0, 8},
   {IDC_FIELD3, field_text, NULL, "No Echo", 90, 6, 4},
   {IDC_FIELD4, field_text, NULL, "No Pending Delete", 160, 16, 10},
 };

 ats_form(&ui, "Form Fill", form, 4, (ui_form_cb)data);
 ats_active(&ui);
 ats_destroy(&ui);
}

void message_box(__ATS_MENU__) {
 ats_buttons buttons = {0};
 char lang_bt_eng[] = "English";

 buttons[0].label = lang_bt_eng;
 int res = ats_message_box("Language",
  "Please choose a language.", buttons, 1);
 printf("messageBox return %d\n", res);

 if (res == 1) {
  buttons[0].label = "No";
  buttons[1].label = "Yes";
  buttons[2].label = "Maybe";
  res = ats_message_box("Answer this question",
   "Do you like to program in C language?", buttons, 3);
  printf("messageBox return %d\n", res);
  if (res == 1) {
   buttons[0].label = "Accept";
   res = ats_message_box("Oops",
    "Unfortunately, you are a bad person.\nThere is nothing I can do for you.", buttons, 1);
   printf("messageBox return %d\n", res);
  }
 }
}

void web_box(__ATS_MENU__) {
 ats_t ui = {0};
 if (ats_webview(&ui, "Webview", "http://en.wikipedia.org/wiki/WebView", 800, 400, true)) {
  ats_webactive(&ui);
  ats_webdestroy(&ui);
 }
}

#define ID_FILE_OPEN 1
#define ID_FILE_FORM  2
#define ID_MODE_ALERT 3
#define ID_MODE_ARCADE 4
#define ID_MODE_KEY  5
#define ID_FILE_SAVE  6
#define ID_WEB_BOX  7

int main(int argc, char **argv) {
 int error = -1;
 ats_t ui = {0};
 if (ats_window(&ui, "Skeleton", 600, 600, false)
  && ats_menubar_set(&ui, 2)) {
  menuitem_t items[] = {
   {ID_FILE_OPEN, "Open", (_menu_cb)ats_open_dialog, "O", NULL},
   {ID_FILE_SAVE, "Save", (_menu_cb)ats_save_dialog, "S", NULL},
   {__ATS_SEPERATOR__},
   {ID_FILE_FORM, "Form", form_prompt, "F", NULL},
  };

  menuitem_t items_two[] = {
   {ID_MODE_ALERT, "Alert Box", message_box, "A", NULL},
   {ID_MODE_ARCADE, "Arcade Box", color_box, "B", NULL},
   {ID_MODE_KEY, "Key Box", key_box, "K", NULL},
   {__ATS_SEPERATOR__},
   {ID_WEB_BOX, "Webview Box", web_box, "W", NULL},
  };

  if (!ats_font_set(&ui, helvetica)
   || !ats_menu_set(&ui, 0, items, 4, 1, "File")
   || !ats_menu_set(&ui, 1, items_two, 5, 2, "Mode")) {
   error = -2;
  }

  if (error == -1)
   error = ats_handler(&ui);

  ats_close(&ui);
 }

 return error;
}
```

## Screenshots

<table>
<tr>
<th>Linux X11 - WSL2</th>
</tr>
<tr>
<td>
<img src="docs/screenshots/X11/1.png" alt="X11" width="320" height="200"/>
<img src="docs/screenshots/X11/2.png" alt="X11" width="320" height="200"/>
<img src="docs/screenshots/X11/3.png" alt="X11" width="320" height="200"/>
<img src="docs/screenshots/X11/4.png" alt="X11" width="320" height="200"/>
<img src="docs/screenshots/X11/5.png" alt="X11" width="320" height="200"/>
<img src="docs/screenshots/X11/6.png" alt="X11" width="320" height="200"/>
<img src="docs/screenshots/X11/7.png" alt="X11" width="320" height="200"/>
<img src="docs/screenshots/X11/8.png" alt="X11" width="320" height="200"/>
</td>
</tr>
</table>

## Contributing

Contributions are encouraged and welcome; I am always happy to get feedback or pull requests on Github :) Create [Github Issues](https://github.com/zelang-dev/athena/issues) for bugs and new features and comment on the ones you are interested in.

## License

The MIT License (MIT). Please see [License File](LICENSE.md) for more information.

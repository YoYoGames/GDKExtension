# GameMaker Studio 2 - GDK Extension

An Extension for GameMaker Studio 2 (GMS2) that gives GMS2 Windows Target support for the GDK allowing them to be released on the Microsoft Store and use XBox Live functionality (for those developers that have access through id@xbox, see [this link](https://www.xbox.com/developers/id) for more information on id@xbox).

For more information on how to use the GDK Extension check our [tech blog](https://www.yoyogames.com/en/blog/gdk-extension) and for extra details on configuring the Partner Center check out our [zendesk article](https://help.yoyogames.com/hc/en-us/articles/4411044955793).

**NOTE**: Only Windows x64 Target is supported by the GDK, ensure that your GMS2 project has the x64 option selected in **Options &#8594; Windows &#8594; General**

# Table Of Contents

- [GameMaker Studio 2 - GDK Extension](#gamemaker-studio-2---gdk-extension)
- [Table Of Contents](#table-of-contents)
	- [Contents of this repository](#contents-of-this-repository)
	- [Building this Extension](#building-this-extension)
	- [Running the GMS2 Project](#running-the-gms2-project)

--- 

## Contents of this repository

This repository contains the source code for the DLL that implements the GDK functionality that is exposed to GameMaker games, it is written in C++. It also contains an example GMS2 project that contains the extension definition and illustrates how to use the extension.

---

## Building this Extension


1. Install VS2019 - see https://visualstudio.microsoft.com/downloads/ 
2. Install GDK - see https://github.com/microsoft/GDK/releases/tag/June_2023
3. Install CMAKE - see https://cmake.org/download/
4. Clone this repository (NOTE: This repository has submodules)
5. Open the Visual Studio 2019
6. Open the Solution in DLL/GDKExtension.sln
7. Go to (Project Properties --> C/C++ -> General -> Additional Include Directories) and add the path: `C:\ProgramData\GameMakerStudio2\Cache\runtimes\<current-runtime>\yyc\include\` (may be different in you system)
8. Select the Debug|Gaming.Desktop.x64 or Release|Gaming.Desktop.x64
9. Build

**NOTE**: Output from this build will be copied into the GMS2 GDK project

---

## Running the GMS2 Project

Open the GMS2 Project in this repository from GDK_Project_GMS2/GDK_Project_GMS2.yyp file.

---

## Documentation

We provide both a PDF version of the documentation included with the extension and inside the demo project (datafiles) and a fully converted version to the [Github Wiki](https://github.com/YoYoGames/GDKExtension/wiki) format (the latter will be the most up-to-date version, the other will follow shortly after). *If there are any PR requests with new feature implementation please make sure you also provide the documentation for the implemented features.*

---

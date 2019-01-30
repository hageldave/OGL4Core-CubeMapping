# OGL4Core-CubeMapping
A demo of cube mapping applications in OpenGL realised as a Plugin to the OGL4Core Framework.

---
## Installation
This Project is a plugin to the OGL4Core framework developed by the University of Stuttgart.

1. In order to run it you need to download the framework here <https://www.vis.uni-stuttgart.de/projekte/ogl4core-framework>. There are versions for differenmt Linux distributions as well as Windows, you need to download a version with sources (src) in order to compile this plugin.
2. Unpack the downloaded archive to a location of your choice.
3. In the root directory of the OGL4Core framework you'll find a directory named `Plugins`. Within that directory, clone this repository. 
E.g. `~/OGL4Core$ cd Plugins/` and then `~/OGL4Core/Plugins$ git clone https://github.com/hageldave/OGL4Core-CubeMapping.git`
4. Building the Plugin:
  * **Linux** `cd` to the cloned repository and call `make` to build the plugin.
E.g. `~/OGL4Core/Plugins$ cd OGL4Core-CubeMapping/` and then `~/OGL4Core/Plugins/OGL4Core-CubeMapping$ make`.
  * **Linux w/ QT Creator** there is a QT project file `CubeMapping.pro` and corresponding project settings file `CubeMapping.pro.user` which can be used to open the project in QT Creator (e.g. [for Ubuntu](https://packages.ubuntu.com/de/bionic/qtcreator) ) and build it from there.
  * **Windows** Unfortunately there is no easy way in Windows, but there are Visual Studio project files which can be used to open the project in [Visual Studio](https://visualstudio.microsoft.com/vs/community/) and build and run it from there. Simply open the `CubeMapping.vcxproj` file with VS.

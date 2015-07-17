# Build Instructions
This project fully supports Linux today.
Support for Windows is for the loader, layers, and the Glave debugger (additional info below).  Additional Windows support will be coming in Q1'15.
Support for Android is TBD.

## Git the Bits

Make sure you have access to the Khronos Github repository. If not, send an email to
jens@lunarg.com or courtney@lunarg.com and we can add you if you have a Khronos account.
Also need to be sure that your Github account name is in your Khronos profile or the
system will disable Github access. Once you have access to the Khronos Github repository,
the preferred work flow is to fork that repo, commit work on to your fork and then issue a
pull request to integrate that work into the Khronos repo. If that's too much, it's okay
to clone the Khronos repository directly.

Note: If you are doing ICD (driver) development, please make sure to look at documentation in the [ICD Loader](loader/README.md) and the [Sample Driver](icd).

## Linux System Requirements
Ubuntu 14.10 needed for DRI 3

```
sudo apt-get install git subversion cmake libgl1-mesa-dev freeglut3-dev libglm-dev libpng12-dev libmagickwand-dev qt5-default libpciaccess-dev libpthread-stubs0-dev
sudo apt-get build-dep mesa
```
Warning: Recent versions of 14.10 have **REMOVED** DRI 3.  
Version: 2:2.99.914-1~exp1ubuntu4.1 is known to work.  
To see status of this package:
```
dpkg -s xserver-xorg-video-intel
```

Note:  
Version 2:2.99.914-1~exp1ubuntu4.2 does not work anymore.  
To install the working driver from launchpadlibrarian.net:  
- Remove the current driver:  
```
sudo apt-get purge xserver-xorg-video-intel
```
- Download the old driver:  
```
wget http://launchpadlibrarian.net/189418339/xserver-xorg-video-intel_2.99.914-1%7Eexp1ubuntu4.1_amd64.deb
```
- Install the driver:
```
sudo dpkg -i xserver-xorg-video-intel_2.99.914-1~exp1ubuntu4.1_amd64.deb
```
- Pin the package to prevent updates
```
sudo bash -c "echo $'Package: xserver-xorg-video-intel\nPin: version 2:2.99.914-1~exp1ubuntu4.1\nPin-Priority: 1001' > /etc/apt/preferences.d/xserver-xorg-video-intel"
```

- Either restart Ubuntu or just X11.  
  
## Clone the repository

To create your local git repository:
```
mkdir YOUR_DEV_DIRECTORY  # it's called GL-Next on Github, but the name doesn't matter
cd YOUR_DEV_DIRECTORY
git clone -o khronos git@github.com:KhronosGroup/GL-Next.git .
# Or substitute the URL from your forked repo for git@github.com:KhronosGroup above.
```

## Linux Build

The sample driver uses cmake and should work with the usual cmake options and utilities.
The standard build process builds the icd, the icd loader and all the tests.

Example debug build:
```
cd YOUR_DEV_DIRECTORY  # cd to the root of the vk git repository
export KHRONOS_ACCOUNT_NAME= <subversion login name for svn checkout of BIL>
./update_external_sources.sh  # fetches and builds glslang, llvm, LunarGLASS, and BIL
cmake -H. -Bdbuild -DCMAKE_BUILD_TYPE=Debug
cd dbuild
make
```

To run VK programs you must tell the icd loader where to find the libraries.
This is described in a specification in the Khronos documentation Git
repository.  See the file:
https://gitlab.khronos.org/vulkan/vulkan/blob/lunarg-sdk-0.2/ecosystem/LinuxICDs.txt

This specification describes both how ICDs and layers should be properly
packaged, and how developers can point to ICDs and layers within their builds.


## Linux Test

The test executibles can be found in the dbuild/tests directory. The tests use the Google
gtest infrastructure. Tests available so far:
- vkinfo: Report GPU properties
- vkbase: Test basic entry points
- vk_blit_tests: Test VK Blits (copy, clear, and resolve)
- vk_image_tests: Test VK image related calls needed by render_test
- vk_render_tests: Render a single triangle with VK. Triangle will be in a .ppm in
the current directory at the end of the test.

## Linux Demos

The demos executables can be found in the dbuild/demos directory. The demos use DRI 3
to render directly onto window surfaces.
- tri: a textured triangle
- cube: a textured spinning cube

## Linux Render Nodes

The render tests depend on access to DRM render nodes.
To make that available, a couple of config files need to be created to set a module option
and make accessible device files.
The system will need to be rebooted with these files in place to complete initialization.
These commands will create the config files.

```
sudo tee /etc/modprobe.d/drm.conf << EOF
# Enable render nodes
options drm rnodes=1
EOF
# this will add the rnodes=1 option into the boot environment
sudo update-initramfs -k all -u
```
```
sudo tee /etc/udev/rules.d/drm.rules << EOF
# Add permissions to render nodes
SUBSYSTEM=="drm", ACTION=="add", DEVPATH=="/devices/*/renderD*", MODE="020666"
EOF
```

## Windows System Requirements

Windows 7+ with additional required software packages:

- Microsoft Visual Studio 2013 Professional.  Note: it is possible that lesser/older versions may work, but that has not been tested.
- CMake (from http://www.cmake.org/download/).  Notes:
  - In order to build the Glave debugger, you need at least version 3.0.
  - Tell the installer to "Add CMake to the system PATH" environment variable.
- Python 3 (from https://www.python.org/downloads).  Notes:
  - Select to install the optional sub-package to add Python to the system PATH environment variable.
  - Need python3.3 or later to get the Windows py.exe launcher that is used to get pyhton3 rather than python2 if both are installed on Windows
- Git (from http://git-scm.com/download/win).
  - Note: If you use Cygwin, you can normally use Cygwin's "git.exe".  However, in order to use the "update_external_sources.bat" script, you must have this version.
  - Tell the installer to allow it to be used for "Developer Prompt" as well as "Git Bash".
  - Tell the installer to treat line endings "as is" (i.e. both DOS and Unix-style line endings).
- Image Magick is used by the tests to compare images (from http://www.imagemagick.org/script/binary-releases.php)
  - Be sure to check box to "Install development headers and libraries"
- glslang is required for demos and tests.
  - You can download and configure it (in a peer directory) here: https://github.com/KhronosGroup/glslang/blob/master/README.md
  - A windows batch file has been included that will pull and build the correct version.  Run it from Developer Command Prompt for VS2013 like so:
    - update_external_sources.bat --build-glslang

Optional software packages:

- Qt 5.3 (from http://www.qt.io/download/).  Notes:
  - Qt 5.3 is required in order to build the Glave debugger (GUI).  The Glave trace and replay tools can be built without Qt, but the debugger/GUI is built on top of Qt 5.3.  Various dependencies, from the Qt package are copied to the directory where the Glave debugger and its libraries are built.  In order to copy and run the debugger in another directory, these libraries must also be copied.  Other notes:
  - While there are commercial licenses, you can also use the "Community" (free) license.
  - By default, the installer will select the latest version (e.g. Qt 5.4) as well as some other components.  You must select "Qt 5.3"!  You can have multiple versions installed (e.g. Qt 5.2.1, 5.3, and 5.4).
  - Installing Qt takes a long time.
- Cygwin (from https://www.cygwin.com/).  Notes:
  - Cygwin provides some Linux-like tools, which are valuable for obtaining the source code, and running CMake.
    Especially valuable are the BASH shell and git packages.
  - If you don't want to use Cygwin, there are other shells and environments that can be used.
    You can also use a Git package that doesn't come from Cygwin.

## Windows Build

Cygwin is used in order to obtain a local copy of the Git repository, and to run the CMake command that creates Visual Studio files.  Visual Studio is used to build the software, and will re-run CMake as appropriate.

Example debug build (e.g. in a "Developer Command Prompt for VS2013" window):
```
cd LoaderAndTools  # cd to the root of the Vulkan git repository
update_external_sources.bat --build-glslang
mkdir _out64
cd _out64
cmake -G "Visual Studio 12 Win64" -DCMAKE_BUILD_TYPE=Debug ..
```

At this point, you can use Windows Explorer to launch Visual Studio by double-clicking on the "VULKAN.sln" file in the \_out64 folder.  Once Visual Studio comes up, you can select "Debug" or "Release" from a drop-down list.  You can start a build with either the menu (Build->Build Solution), or a keyboard shortcut (Ctrl+Shift+B).  As part of the build process, Python scripts will create additional Visual Studio files and projects, along with additional source files.  All of these auto-generated files are under the "_out64" folder.

VK programs must be able to find and use the VK.dll libary. Make sure it is either installed in the C:\Windows\System32 folder, or the PATH enviroment variable includes the folder that it is located in.

To run VK programs you must tell the icd loader where to find the libraries.
This is described in a specification in the Khronos documentation Git
repository.  See the file:
https://gitlab.khronos.org/vulkan/vulkan/blob/lunarg-sdk-0.2/ecosystem/WindowsICDs.txt

This specification describes both how ICDs and layers should be properly
packaged, and how developers can point to ICDs and layers within their builds.

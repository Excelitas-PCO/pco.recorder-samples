# pco.recorder-samples
This project contains different sample projects showing how to use Excelitas PCO's pco.recorder package,   
which can be downloaded here: [pco.recorder](https://www.excelitas.com/de/product/pco-software-development-kits#custom-tab-general-sdk)

If you are looking for a more high-level, easier, API, please have a look at our
[pco.cpp](https://www.excelitas.com/product/pco-software-development-kits#custom-tab-c__) package and the corresponding
[pco.cpp-samples](https://github.com/Excelitas-PCO/pco.cpp-samples) on GitHub.

## Requirements
- Windows or Linux 

When using "x64 Native Tools Command Prompt for VS 2022" 
- Visual Studio 2019/2022

or when using a terminal
- Ninja Make
- CMake
- Any CPP Compiler, e.g. minGW

## Project Structure
 
```
.gitignore
README.md
LICENSE
CMakePresets.json
CMakeLists.txt
- doc
  - res
- externals
  - pco
- src
  - ColorConvertExample
  - MultiCameraExample
  - SimpleExample
  - SimpleExample_CamRam
  - SimpleExample_FIFO
```

**CMakeLists.txt** is the main cmake file and **CMakePresets.json** contains already predefined presets for building debug and release,
both on windows and linux platforms

All examples are in the **src** subfolder.  
The **externals/pco** folder contains also a **CMakeLists.txt** file which handles the pco.recorder dependencies

## Sample Description

### SimpleExample

This is a small console application which
1. Opens a camera
2. Sets an exposure time
3. Records a sequence of images
   - Start Record
   - Wait until finished
4. For all recorded images
  - Copy the image from recorder
  - Export the recorded images as 16bit tif files 

**Note**: This way of saving image is only for a small amount of images / snapshots. 
If you need to store lots of images as files, either consider using our file modes
(description can be found in the [pco.recorder manual](https://www.excelitas.com/de/de/file-download/download/public/103154?filename=pco_recorder_Manual.pdf))  
Or leverage any open source library for storing image files, e.g. OpenCV.

### SimpleExample_FIFO

This example is similar to **SimpleExample** but uses the ```PCO_RECORDER_MEMORY_FIFO``` instead of ```PCO_RECORDER_MEMORY_SEQUENCE```,
so that the images are automatically read in a sequential order.

### SimpleExample_CamRam

This example is similar to **SimpleExample** but adapted to the workflow of PCO cameras with internal memory. 
Here you can only get a live image during record (using ```PCO_RECORDER_LATEST_IMAGE```) and read the actual images directly from the cameras internal memory when record is stopped.  

### ColorConvertExample

This example is similar to **SimpleExample** but additionally it leverages our **pco.convert** color conversion library to create and save color images.  
The example shows a default preparation and setup of the color conversion 

**Note**: This example is only useful for color cameras, if you want to use it for monochrome cameras you need to use ```PCO_Convert16TOPSEUDO``` instead of ```PCO_Convert16TOCOL``` 

### MultiCameraExample

This example shows how to work with two cameras using one pco.recorder instance.

The following is done: 
1. Open both cameras
2. Set default settings and trigger mode
3. Start record
4. Record images synchronously by using ```PCO_ForceTrigger```
5. Save the first recorded images for both cameras

**Note**: The example uses soft trigger and the ```PCO_ForceTrigger``` command to synchronize between the two cameras. 
You could of course also start the cameras without triggering, but in many applications where multiple cameras are used, it is needed to sync the image acquisition.  
If you need very accurate synchronization, we highly recommend using external trigger signals and configure the camera to use hardware trigger, since this is the most accurate synchronization.


## Installation

To use this example project you can either clone, fork or download the source code. 
After you have configured everything to your needs you can simply configure, build and install it using cmake.

### Configuration

The **CMakePresets.json** contain already predefined configurations for cmake builds on windows and linux.  

Beside of the preset name and description we have the following variables which you can configure to your needs: 

#### generator 
Here we use *Ninja* as it is available both on linux and on windows systems, but you can of course change this

#### binaryDir
This defines where the build files go to.  
Our default here is *<preset name>/build*, so e.g. *release_lnx/build* for the *release_lnx* preset

#### CMAKE_BUILD_TYPE
Build type. This matches with our preset names

#### CMAKE_INSTALL_PREFIX
This defines where the files should be installed to when calling ```cmake --install```
Our default here is *<preset name>/install*, so e.g. *release_lnx/install* for the *release_lnx* preset

#### PCO_PACKAGE_INSTALL_DIR
This specifies the root path to your installation of the **pco.recorder** package.  
Our default here is the system wide installation path, so normally you do not need to change it.  
If you installed pco.recorder on windows as user, you need to adapt this to the actual installation path of pco.recorder

#### AUTO_UPDATE_PCO_PACKAGE
If this flag is set to true, the *./externals/pco/CMakeLists.txt* will automatically update the **pco.recorder** related files from the pco.recorder install path, e.g. when you install a new version of pco.recorder the examples will automatically be updated on the next reconfiguration.

If you want to disable this mechanism, just set ```"AUTO_UPDATE_PCO_PACKAGE": false``` 

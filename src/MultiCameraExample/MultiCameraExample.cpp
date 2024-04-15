#include <iostream>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <chrono>

#ifdef PCO_LINUX
#include <pco_linux_defs.h>
#include <sc2_sdkaddendum.h>
#include <pco_device.h>
#include <pco_camexport.h>
#else
#define NOMINMAX

#include <Windows.h>
#include <tchar.h>
#endif

//SDK Includes
#define PCO_SENSOR_CREATE_OBJECT //To get PCO_SENSOR_TYPE_DEF
#include <sc2_defs.h>
#include <sc2_common.h>
#include <pco_err.h>
#include <sc2_sdkstructures.h>
#include <sc2_camexport.h>

//Recorder Includes
#include <pco_recorder_export.h>
#include <pco_recorder_defines.h>

#define CAMCOUNT    2

// This functions shows how you can sort cameras according to e.g.serial number
// Here we s std::map to do the sorting in an ascending order
void sortCamerasBySN(HANDLE* hCamArr, int len)
{
  std::map<DWORD, HANDLE> info_map;
  for (int i = 0; i < len; ++i)
  {
    PCO_CameraType camType;
    camType.wSize = sizeof(PCO_CameraType);
    PCO_GetCameraType(hCamArr[i], &camType);
    info_map[camType.dwSerialNumber] = hCamArr[i];
  }

  int i = 0;
  for (auto& entry : info_map)
  {
    hCamArr[i++] = entry.second;
  }
}

int main()
{
  int err;
  err = PCO_InitializeLib();
  if (err)
  {
    return err;
  }

  HANDLE hRec = nullptr;
  HANDLE hCamArr[CAMCOUNT];
  DWORD imgDistributionArr[CAMCOUNT];
  DWORD maxImgCountArr[CAMCOUNT];
  DWORD reqImgCountArr[CAMCOUNT];

  //Some frequently used parameters for the camera
  DWORD numberOfImages = 10;
  DWORD expTime = 10;
  WORD expBase = TIMEBASE_MS;
  //we use software trigger here
  WORD triggerMode = TRIGGER_MODE_SOFTWARETRIGGER;


  //open all cameras
  PCO_OpenStruct camstruct;
  for (int i = 0; i < CAMCOUNT; i++)
  {
    hCamArr[i] = 0;
    //Reset open struct to scan all interfaces
    memset(&camstruct, 0, sizeof(camstruct));
    camstruct.wSize = sizeof(PCO_OpenStruct);
    camstruct.wInterfaceType = 0xFFFF;

    //open next camera
    err = PCO_OpenCameraEx(&hCamArr[i], &camstruct);
    if (err != PCO_NOERROR)
    {
      printf("Camera number %i not found\n", i);
      printf("Press <Enter> to end\n");
      err = getchar();
      PCO_CleanupLib();
      return -1;
    }
    // Do some settings to prepare each camera
    // The following functions show only a very small subset of options

    //Make sure recording is off
    err = PCO_SetRecordingState(hCamArr[i], 0);
    // Reset to default
    err = PCO_ResetSettingsToDefault(hCamArr[i]);
    //Do some settings
    err = PCO_SetTimestampMode(hCamArr[i], TIMESTAMP_MODE_BINARYANDASCII);
    err = PCO_SetBitAlignment(hCamArr[i], BIT_ALIGNMENT_LSB);
    err = PCO_SetDelayExposureTime(hCamArr[i], 0, expTime, TIMEBASE_MS, expBase);
    err = PCO_SetTriggerMode(hCamArr[i], triggerMode);
    // Activate metadata
    WORD metaSize = 0, metaVersion = 0;
    err = PCO_SetMetaDataMode(hCamArr[i], METADATA_MODE_ON, &metaSize, &metaVersion);
    //Arm camera after all settings are done
    err = PCO_ArmCamera(hCamArr[i]);
  }
  //Sort by SN
  sortCamerasBySN(hCamArr, CAMCOUNT);

  // Set image distribution to 1 to give every camera identical space for images in PC RAM
  // If you know in advance, that e.g camera 2 records twice as much images as 1,
  // then you can e.g set this here to 2 for camera 2
  for (int i = 0; i < CAMCOUNT; i++)
    imgDistributionArr[i] = 1;

  //Reset Recorder to make sure a no previous instance is running
  err = PCO_RecorderResetLib(false);

  //Create Recorder (mode: memory sequence)
  // With this mode parameter you can choose where to store the images
  // PCO_RECORDER_MODE_MEMORY -> PC RAM
  // PCO_RECORDER_MODE_FILE -> PC Harddisk as file(s)
  // PCO_RECORDER_MODE_CAMRAM -> Don't store on PC at all, use camera internal memory (only if camera supports this)
  WORD mode = PCO_RECORDER_MODE_MEMORY;
  err = PCO_RecorderCreate(&hRec, hCamArr, imgDistributionArr, CAMCOUNT, mode, "C", maxImgCountArr);

  // Set required images for each cameras
  // In this example we suppose same count for all cameras
  for (int i = 0; i < CAMCOUNT; i++)
  {
    reqImgCountArr[i] = numberOfImages;
    if (reqImgCountArr[i] > maxImgCountArr[i])
      reqImgCountArr[i] = maxImgCountArr[i];
  }

  //Init Recorder
  // With this type parameter you can choose the recorder type you want to use
  // Functionality depends on the selected mode in PCO_RecorderCreate
  // For mode = PCO_RECORDER_MODE_MEMORY you can select the internal buffer organization (sequence, ring or fifo)
  // For mode = PCO_RECORDER_MODE_FILE you can choose the file type ((multi)tif, (multi)dicom, b16, pcoraw)
  // For mode = PCO_RECORDER_MODE_CAMRAM you can define if you are going to read images in sequential order or not 
  //            (Sequential order will only do some small internal performance optimizations to read seqential images faster)
  WORD type = PCO_RECORDER_MEMORY_SEQUENCE;
  err = PCO_RecorderInit(hRec, reqImgCountArr, CAMCOUNT, type, 0, nullptr, nullptr);

  //Start all cameras
  err = PCO_RecorderStartRecord(hRec, nullptr);

  // Wait until acquisition is finished
  // Send Softwaretrigger every 500 ms
  bool acquisitionRunning = true;
  DWORD procImgCount = 0;
  while (acquisitionRunning)
  {
    //Send trigger commands
    WORD triggered = 0; //holds flag if trigger was successful
    for (int i = 0; i < CAMCOUNT; i++)
      PCO_ForceTrigger(hCamArr[i], &triggered);

    std::this_thread::sleep_for(std::chrono::milliseconds((500)));

    //Check how the cameras are performing
    DWORD healthWarn = 0, healthErr = 0, status = 0;
    bool runState = false;
    acquisitionRunning = false;
    for (int i = 0; i < CAMCOUNT; i++)
    {
      //Regularly checking camera health is also a good idea
      err = PCO_GetCameraHealthStatus(hCamArr[i], &healthWarn, &healthErr, &status);
      if (healthErr != PCO_NOERROR) //Stop only the camera that shows health error
        PCO_RecorderStopRecord(hRec, hCamArr[i]);

      err = PCO_RecorderGetStatus(hRec, hCamArr[i], &runState,
        nullptr, nullptr, &procImgCount, nullptr, nullptr, nullptr, nullptr, nullptr);

      printf("Camera %i has image count %d \n", i, procImgCount);

      //we loop until as long as at least one camera runs
      acquisitionRunning |= runState;
    }
  }

  //////////////////////////////////////////////
  //TODO: Process, Save or analyze the image(s)
  //Here we just read, save the first image of every camera with metadata to tif file
  //////////////////////////////////////////////

  // Read and save first image of every camera
  // Only for demonstration, not very performant
  for (int i = 0; i < CAMCOUNT; i++)
  {
    //Get image size of e.g. first camera
    WORD imgWidth = 0, imgHeight = 0;
    err = PCO_RecorderGetSettings(hRec, hCamArr[i], nullptr, nullptr, nullptr, &imgWidth, &imgHeight, nullptr);

    //Allocate memory for one image
    WORD* imgBuffer = nullptr;
    imgBuffer = new WORD[(__int64)imgWidth * (__int64)imgHeight];
    // Quit program if memory could not be allocated
    if (imgBuffer == nullptr)
    {
      PCO_CleanupLib();
      return 1;
    }

    //Get image with metadata
    PCO_METADATA_STRUCT metadata;
    memset(&metadata, 0, sizeof(PCO_METADATA_STRUCT));
    metadata.wSize = sizeof(PCO_METADATA_STRUCT);
    err = PCO_RecorderCopyImage(hRec, hCamArr[i], 0, 1, 1, imgWidth, imgHeight, imgBuffer, nullptr, &metadata, nullptr);

    std::string filename = "test_cam" + std::to_string(i) + ".tif";
    if (err == PCO_NOERROR)
      err = PCO_RecorderSaveImage(imgBuffer, imgWidth, imgHeight, FILESAVE_IMAGE_BW_16, false, filename.c_str(), true, &metadata);

    //free the allocated buffer
    delete[] imgBuffer;
  }


  //Delete Recorder
  err = PCO_RecorderDelete(hRec);
  //Close cameras
  for (int i = 0; i < CAMCOUNT; i++)
    err = PCO_CloseCamera(hCamArr[i]);

  PCO_CleanupLib();
  return 0;
}

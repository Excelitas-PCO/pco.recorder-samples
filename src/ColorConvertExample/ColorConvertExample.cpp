#include <iostream>
#include <cstring>
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

//Convert Includes
#include <pco_color_corr_coeff.h>
#include <pco_convexport.h>
#include <pco_convstructures.h>

#define CAMCOUNT    1

//Compute the color mode according to pattern and x0 and y0
int getColorMode(WORD colPattern, WORD roiX0, WORD roiY0)
{
  //Find position of red nibble
    //////////////////////////////////////////////////////////////////////////////
  int imodes[4][2][2] = { 3,2,1,0, // upper left is red
                          2,3,0,1, // upper left is green in red line
                          1,0,3,2, // upper left is green in blue line
                          0,1,2,3 };// upper left is blue

  int color_mode = colPattern & 0x0F;
  if ((color_mode < 1) || (color_mode > 4))
    color_mode = 1;

  color_mode--;  // zero based index!
  color_mode = imodes[color_mode][roiY0 & 0x01][roiX0 & 0x01];

  return color_mode;
}

int main()
{
  int iRet;
  iRet = PCO_InitializeLib();
  if (iRet)
  {
    return iRet;
  }

  HANDLE hRec = NULL;
  HANDLE hConv = NULL;
  HANDLE hCamArr[CAMCOUNT];
  DWORD imgDistributionArr[CAMCOUNT];
  DWORD maxImgCountArr[CAMCOUNT];
  DWORD reqImgCountArr[CAMCOUNT];

  //Some frequently used parameters for the camera
  DWORD numberOfImages = 10;
  DWORD expTime = 10;
  WORD expBase = TIMEBASE_MS;
  WORD metaSize = 0, metaVersion = 0;

  //Open camera and set to default state
  PCO_OpenStruct camstruct;
  memset(&camstruct, 0, sizeof(camstruct));
  camstruct.wSize = sizeof(PCO_OpenStruct);
  //set scanning mode
  camstruct.wInterfaceType = 0xFFFF;

  hCamArr[0] = 0;
  //open next camera
  iRet = PCO_OpenCameraEx(&hCamArr[0], &camstruct);
  if (iRet != PCO_NOERROR)
  {
    printf("No camera found\n");
    printf("Press <Enter> to end\n");
    iRet = getchar();
    PCO_CleanupLib();
    return -1;
  }
  //Make sure recording is off
  iRet = PCO_SetRecordingState(hCamArr[0], 0);
  //Do some settings
  iRet = PCO_SetTimestampMode(hCamArr[0], TIMESTAMP_MODE_OFF);
  iRet = PCO_SetMetaDataMode(hCamArr[0], METADATA_MODE_ON,
    &metaSize, &metaVersion);
  iRet = PCO_SetBitAlignment(hCamArr[0], BIT_ALIGNMENT_LSB);
  //Set Exposure time
  iRet = PCO_SetDelayExposureTime(hCamArr[0], 0, expTime,
    2, expBase);
  //Arm camera
  iRet = PCO_ArmCamera(hCamArr[0]);

  //Set up color conversion
  ///////////////////////////////////////////////////////
  WORD alignment;
  iRet = PCO_GetBitAlignment(hCamArr[0], &alignment);

  //Get Camera Type
  WORD cameraType;
  PCO_CameraType camTypeStruct;
  camTypeStruct.wSize = sizeof(PCO_CameraType);
  iRet = PCO_GetCameraType(hCamArr[0], &camTypeStruct);
  cameraType = camTypeStruct.wCamType;

  //Get Sensor description
  WORD dynRes;
  WORD sensorTypeDesc;
  PCO_Description descStruct;
  descStruct.wSize = sizeof(PCO_Description);
  iRet = PCO_GetCameraDescription(hCamArr[0], &descStruct);

  sensorTypeDesc = descStruct.wSensorTypeDESC;
  dynRes = descStruct.wDynResDESC;

  //Get ROI
  WORD roiX0 = 0, roiY0 = 0, roiX1 = 0, roiY1 = 0;
  iRet = PCO_GetROI(hCamArr[0], &roiX0, &roiY0, &roiX1, &roiY1);

  //Get CCM
  double ccm[9];
  iRet = PCO_GetColorCorrectionMatrix(hCamArr[0], ccm);

  //Set sensor info Bits
  int sensorInfoBits = CONVERT_SENSOR_COLORIMAGE;
  if (alignment == BIT_ALIGNMENT_MSB)
    sensorInfoBits |= CONVERT_SENSOR_UPPERALIGNED;

  //Get Dark Offset depending on camera family
  int darkOffset = 100;
  if (((cameraType & 0xFF00) == CAMERATYPE_PCO1200HS) ||
    ((cameraType & 0xFF00) == CAMERATYPE_PCO_DIMAX_STD))
    darkOffset = 32;

  PCO_SensorInfo sensorStruct;
  sensorStruct.wSize = sizeof(PCO_SensorInfo);
  sensorStruct.iConversionFactor = 0;
  sensorStruct.iDataBits = dynRes;
  sensorStruct.iSensorInfoBits = sensorInfoBits;
  sensorStruct.iDarkOffset = darkOffset;
  sensorStruct.strColorCoeff.da11 = ccm[0];
  sensorStruct.strColorCoeff.da12 = ccm[1];
  sensorStruct.strColorCoeff.da13 = ccm[2];
  sensorStruct.strColorCoeff.da21 = ccm[3];
  sensorStruct.strColorCoeff.da22 = ccm[4];
  sensorStruct.strColorCoeff.da23 = ccm[5];
  sensorStruct.strColorCoeff.da31 = ccm[6];
  sensorStruct.strColorCoeff.da32 = ccm[7];
  sensorStruct.strColorCoeff.da33 = ccm[8];
  sensorStruct.iCamNum = 0;
  sensorStruct.hCamera = hCamArr[0];

  //Create Convert
  iRet = PCO_ConvertCreate(&hConv, &sensorStruct, PCO_COLOR_CONVERT);

  //This part is optional, only to create a nice looking image, adapt as needed
  //Get display struct
  PCO_Display strDisplay;
  strDisplay.wSize = sizeof(PCO_Display);
  iRet = PCO_ConvertGetDisplay(hConv, &strDisplay);

  //Update display information
  strDisplay.dwProcessingFlags |= PROCESS_REC2020;
  strDisplay.iColor_saturation = 15; //15% Saturation is just a default value
  iRet = PCO_ConvertSetDisplay(hConv, &strDisplay);
  ///////////////////////////////////////////////////////

  //Set image distribution to 1 since only one camera is used
  imgDistributionArr[0] = 1;

  //Reset Recorder to make sure a no previous instance is running
  iRet = PCO_RecorderResetLib(false);

  //Create Recorder (mode: memory sequence)
  WORD mode = PCO_RECORDER_MODE_MEMORY;
  iRet = PCO_RecorderCreate(&hRec, hCamArr, imgDistributionArr,
    CAMCOUNT, mode, "C", maxImgCountArr);

  //Set required images
  reqImgCountArr[0] = numberOfImages;
  if (reqImgCountArr[0] > maxImgCountArr[0])
    reqImgCountArr[0] = maxImgCountArr[0];

  //Init Recorder
  iRet = PCO_RecorderInit(hRec, reqImgCountArr, CAMCOUNT,
    PCO_RECORDER_MEMORY_SEQUENCE, 0, NULL, NULL);

  //Get image size
  WORD imgWidth = 0, imgHeight = 0;
  iRet = PCO_RecorderGetSettings(hRec, hCamArr[0], NULL, NULL,
    NULL, &imgWidth, &imgHeight, NULL);

  //Start camera
  iRet = PCO_RecorderStartRecord(hRec, NULL);

  //Wait until acquisition is finished
  //(all other parameters are ignored)
  bool acquisitionRunning = true;
  while (acquisitionRunning)
  {
    iRet = PCO_RecorderGetStatus(hRec, hCamArr[0],
      &acquisitionRunning,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    DWORD warn = 0, err = 0, status = 0;
    iRet = PCO_GetCameraHealthStatus(hCamArr[0],
      &warn, &err, &status);
    if (err != PCO_NOERROR) //Stop record on health error
      PCO_RecorderStopRecord(hRec, hCamArr[0]);

    std::this_thread::sleep_for(std::chrono::milliseconds((100)));
  }

  //Allocate memory for one image and one color image
  WORD* imgBuffer = NULL;
  imgBuffer = new WORD[(__int64)imgWidth * (__int64)imgHeight];

  BYTE* colorImgBuffer = NULL;
  colorImgBuffer = new BYTE[(__int64)imgWidth * (__int64)imgHeight * 3];

  //Get number of finally recorded images
  DWORD procImgCount = 0;
  iRet = PCO_RecorderGetStatus(hRec, hCamArr[0], NULL, NULL, NULL,
    &procImgCount, NULL, NULL, NULL, NULL, NULL);

  //Get the images and print image counter
  PCO_METADATA_STRUCT metadata;
  metadata.wSize = sizeof(PCO_METADATA_STRUCT);
  bool imageSaved = false;
  DWORD imgNumber = 0;
  for (DWORD i = 0; i < procImgCount; i++)
  {
    iRet = PCO_RecorderCopyImage(hRec, hCamArr[0], i,
      1, 1, imgWidth, imgHeight, imgBuffer,
      &imgNumber, &metadata, NULL);
    if (iRet == PCO_NOERROR)
    {
      printf("Image Number: %d \n", imgNumber);

      //Convert to color
      int colorMode = getColorMode(descStruct.wColorPatternDESC, roiX0, roiY0);
      //Note if you use a soft roi in PCO_RecorderCopyImage you will have to consider this also

      //Only some default processing features the should produce a quite "nice looking" color image, adapt as needed
      //Compare the color conversion dialog in pco.camware
      int mode = CONVERT_MODE_OUT_DOADSHARPEN |
        CONVERT_MODE_OUT_FLIPIMAGE |
        CONVERT_MODE_OUT_DOADSHARPEN |
        CONVERT_MODE_OUT_DOPCODEBAYER |
        CONVERT_MODE_OUT_DOBLUR;

      //Now you will get an bottom up image with bgr (= Bitmap style)
      //This can directly be saved
      //If you need RGB Top Bottom you have to remove the CONVERT_MODE_OUT_FLIPIMAGE and switch colors after conversion manually
      iRet = PCO_Convert16TOCOL(hConv, mode, colorMode, imgWidth, imgHeight, imgBuffer, colorImgBuffer);

      //Save first color image as tiff in the binary folder
      //just to have some output
      if ((iRet == PCO_NOERROR) && (!imageSaved))
      {
        iRet = PCO_RecorderSaveImage(colorImgBuffer,
          imgWidth, imgHeight, FILESAVE_IMAGE_BGR_8,
          true, "test.tif", true, &metadata);
        if (iRet == PCO_NOERROR)
          imageSaved = true;
      }
    }
  }
  delete[] imgBuffer;
  delete[] colorImgBuffer;

  //Delete Convert
  iRet = PCO_ConvertDelete(hConv);
  //Delete Recorder
  iRet = PCO_RecorderDelete(hRec);
  //Close camera
  iRet = PCO_CloseCamera(hCamArr[0]);

  PCO_CleanupLib();
  return 0;
}

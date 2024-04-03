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

#define CAMCOUNT    1
int main()
{
    int iRet;
    iRet = PCO_InitializeLib();
    if (iRet)
    {
        return iRet;
    }

    HANDLE hRec = NULL;
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
    std::memset(&camstruct, 0, sizeof(camstruct));
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

    //Allocate memory for one image
    WORD* imgBuffer = NULL;
    imgBuffer = new WORD[(__int64)imgWidth * (__int64)imgHeight];

    //Get number of finally recorded images
    DWORD procImgCount = 0;
    iRet = PCO_RecorderGetStatus(hRec, hCamArr[0], NULL, NULL, NULL,
        &procImgCount, NULL, NULL, NULL, NULL, NULL);

    //////////////////////////////////////////////
    //TODO: Process, Save or analyze the image(s)
    //Here we just read, print image counter and save one tif file
    //////////////////////////////////////////////

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

            //Save first image as tiff in the binary folder
            //just to have some output
            if (!imageSaved)
            {
                iRet = PCO_RecorderExportImage(hRec, hCamArr[0], i, "test.tif", true);
                if (iRet == PCO_NOERROR)
                    imageSaved = true;
            }
        }
    }
    delete[] imgBuffer;
    //Delete Recorder
    iRet = PCO_RecorderDelete(hRec);
    //Close camera
    iRet = PCO_CloseCamera(hCamArr[0]);
    
    PCO_CleanupLib();
    return 0;
}

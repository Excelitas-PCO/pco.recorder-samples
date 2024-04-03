#include <iostream>
#include <cstring>


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

    DWORD procImgCount;
    WORD ramSegment = 1;

    //Some frequently used parameters for the camera
    DWORD numberOfImages = 10;
    DWORD expTime = 100;
    WORD expBase = TIMEBASE_US;
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
    //switch to sequence mode
    iRet = PCO_SetRecorderSubmode(hCamArr[0], 1);
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

    //Create Recorder (mode: cam ram)
    WORD mode = PCO_RECORDER_MODE_CAMRAM;
    iRet = PCO_RecorderCreate(&hRec, hCamArr, imgDistributionArr,
        CAMCOUNT, mode, "C", maxImgCountArr);

    //Set required images
    reqImgCountArr[0] = numberOfImages;
    if (reqImgCountArr[0] > maxImgCountArr[0])
        reqImgCountArr[0] = maxImgCountArr[0];

    //Init Recorder for segment 1 as example, for sequential readout
    iRet = PCO_RecorderInit(hRec, reqImgCountArr, CAMCOUNT,
        PCO_RECORDER_CAMRAM_SEQUENTIAL, 0, NULL, &ramSegment);

    //Get number of images already in cameras internal memory
    iRet = PCO_RecorderGetStatus(hRec, hCamArr[0], NULL, NULL, NULL,
        &procImgCount, NULL, NULL, NULL, NULL, NULL);

    //Get width and height to allocate memory
    WORD imgWidth = 0, imgHeight = 0;
    iRet = PCO_RecorderGetSettings(hRec, hCamArr[0], NULL,
        &maxImgCountArr[0], NULL, &imgWidth, &imgHeight, NULL);

    //Allocate memory for image
    WORD* imgBuffer = NULL;
    imgBuffer = new WORD[(__int64)imgWidth * (__int64)imgHeight];

    if (procImgCount > 0)
    {
        //If there are already images in the ram segment,
        //you can read them without any previous recording
        // Note: CopyImage is indexed based, so this starts with 0
        iRet = PCO_RecorderCopyImage(hRec, hCamArr[0], 0,
            1, 1, imgWidth, imgHeight, imgBuffer, NULL, NULL, NULL);

        //////////////////////////////////////////////
        //TODO: Process, Save or analyze the image(s)
        //////////////////////////////////////////////
    }

    //Start camera
    iRet = PCO_RecorderStartRecord(hRec, NULL);

    //Wait as long as you want (i.e. for some external event)
    int waitTime = 0;
    while (waitTime < 10)
    {
        //If required you can get a live stream during record
        //(only PCO_RECORDER_LATEST_IMAGE is allowed during record)
        iRet = PCO_RecorderCopyImage(hRec, hCamArr[0],
            PCO_RECORDER_LATEST_IMAGE,
            1, 1, imgWidth, imgHeight, imgBuffer, NULL, NULL, NULL);
        waitTime++;
    }
    //Stop record
    iRet = PCO_RecorderStopRecord(hRec, hCamArr[0]);

    //Get number of finally recorded images
    iRet = PCO_RecorderGetStatus(hRec, hCamArr[0], NULL, NULL, NULL,
        &procImgCount, NULL, NULL, NULL, NULL, NULL);

    //////////////////////////////////////////////
    //TODO: Process, Save or analyze the image(s)
    // Here we just read, print image counter and save one tif file
    //////////////////////////////////////////////

    PCO_METADATA_STRUCT metadata;
    metadata.wSize = sizeof(PCO_METADATA_STRUCT);
    DWORD imgNumber = 0;
    bool imageSaved = false;
    //Get the first "numberOfImages" images from
    //the cameras internal memory
    for (int i = 0; i < (int)numberOfImages; i++)
    {
        //Copy the image at index 5 into the buffer
        iRet = PCO_RecorderCopyImage(hRec, hCamArr[0], i,
            1, 1, imgWidth, imgHeight,
            imgBuffer, &imgNumber, &metadata, NULL);
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

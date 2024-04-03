#include <iostream>
#include <cstring>
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
#define RECORD_TIME_IN_S 5
int main()
{
    int iRet;
    HANDLE hRec = NULL;
    HANDLE hCamArr[CAMCOUNT];
    DWORD imgDistributionArr[CAMCOUNT];
    DWORD maxImgCountArr[CAMCOUNT];
    DWORD reqImgCountArr[CAMCOUNT];

    //Some frequently used parameters for the camera
    DWORD numberOfImages = 100;
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
        PCO_RECORDER_MEMORY_FIFO, 0, NULL, NULL);

    //Get image size
    WORD imgWidth = 0, imgHeight = 0;
    iRet = PCO_RecorderGetSettings(hRec, hCamArr[0], NULL, NULL,
        NULL, &imgWidth, &imgHeight, NULL);

    //Allocate memory for one image
    WORD* imgBuffer = NULL;
    imgBuffer = new WORD[(__int64)imgWidth * (__int64)imgHeight];

    //Get the images currently available
    DWORD procImgCount = 0;
    PCO_METADATA_STRUCT metadata;
    metadata.wSize = sizeof(PCO_METADATA_STRUCT);
    bool imageSaved = false;
    DWORD imgNumber = 0;
    bool isRunning = true;

    //////////////////////////////////////////////
    //TODO: Process, Save or analyze the image(s) during acquisition
    //Here we just read, print image counter and save one tif file
    //////////////////////////////////////////////

    //Start Record
    iRet = PCO_RecorderStartRecord(hRec, nullptr);
    auto start_time = std::chrono::high_resolution_clock::now();
    auto record_time = std::chrono::seconds(RECORD_TIME_IN_S);
    while (isRunning)
    {
        iRet = PCO_RecorderGetStatus(hRec, hCamArr[0], &isRunning,
            NULL, NULL, &procImgCount,
            NULL, NULL, NULL, NULL, NULL);
        if (procImgCount > 0)
        {
            iRet = PCO_RecorderCopyImage(hRec, hCamArr[0], 0,
                1, 1, imgWidth, imgHeight, imgBuffer,
                &imgNumber, &metadata, NULL);
            if (iRet != PCO_NOERROR)
            {
                printf("Error in copy image: %x\n", iRet);
                PCO_RecorderStopRecord(hRec, nullptr);

                break;  //Break on error
            }
            printf("Fill level: %d \tImage Number: %d\n",
                procImgCount, imgNumber);

            // Save the image we have copied above as tiff in the binary folder
            // just to have some output
            // For Fifo the PCO_RecorderExportImage will always save the image you received 
            // in the last PCO_RecorderCopyImage call
            if (!imageSaved)
            {
                iRet = PCO_RecorderExportImage(hRec, hCamArr[0], 0, "test.tif", true);
                if (iRet == PCO_NOERROR)
                    imageSaved = true;
            }
        }

        //Stop on time elapsed
        if (std::chrono::high_resolution_clock::now() - start_time > record_time)
        {
            PCO_RecorderStopRecord(hRec, nullptr);
        }
    }

    delete[] imgBuffer;
    //Delete Recorder
    iRet = PCO_RecorderDelete(hRec);
    //Close camera
    iRet = PCO_CloseCamera(hCamArr[0]);

    return 0;
}

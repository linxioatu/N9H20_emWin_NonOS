/****************************************************************************
 * @file     main.c
 * @version  V1.00
 * $Revision: 4 $
 * $Date: 18/04/25 11:43a $
 * @brief    Mass Storage Class Device sample file
 *
 * @note
 * Copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "N9H20.h"

//#define DETECT_USBD_PLUG
//#define NON_BLOCK_MODE
//#define SUSPEND_POWERDOWN

void Demo_PowerDownWakeUp(void);

INT SpiFlashOpen(UINT32 FATOffset);

BOOL PlugDetection(VOID)
{
#ifdef DETECT_USBD_PLUG
    return udcIsAttached();
#else
    return TRUE;
#endif
}

#ifndef __RAM_DISK_ONLY__
#ifdef __NAND__
NDISK_T MassNDisk;

NDRV_T _nandDiskDriver0 = 
{
    nandInit0,
    nandpread0,
    nandpwrite0,
    nand_is_page_dirty0,
    nand_is_valid_block0,
    nand_ioctl,
    nand_block_erase0,
    nand_chip_erase0,
    0
};
#endif
#endif
INT main(VOID)
{
#ifdef __NAND__
    UINT32 block_size, free_size, disk_size, u32TotalSize;
#endif
#if defined(__SD__) || defined (__SPI_ONLY__) 
    INT32 status;
#endif

    WB_UART_T uart;
    UINT32 u32ExtFreq;
    u32ExtFreq = sysGetExternalClock();
    sysUartPort(1);
    uart.uiFreq = u32ExtFreq*1000;
    uart.uiBaudrate = 115200;
    uart.uiDataBits = WB_DATA_BITS_8;
    uart.uiStopBits = WB_STOP_BITS_1;
    uart.uiParity = WB_PARITY_NONE;
    uart.uiRxTriggerLevel = LEVEL_1_BYTE;
    sysInitializeUART(&uart);

    sysSetSystemClock(eSYS_UPLL,    /* E_SYS_SRC_CLK eSrcClk */
                    192000,         /* UINT32 u32PllKHz */
                    192000,         /* UINT32 u32SysKHz */
                    192000,         /* UINT32 u32CpuKHz */
                    192000/2,       /* UINT32 u32HclkKHz */
                    192000/4);      /* UINT32 u32ApbKHz */

    /* Enable USB */
    udcOpen();
#if !defined(__RAM_DISK_ONLY__) && !defined (__SPI_ONLY__)
    /* initialize FMI (Flash memory interface controller) */
    sicIoctl(SIC_SET_CLOCK, 192000, 0, 0);  /* clock from PLL */

    sicOpen();

#ifdef __NAND__
    /* initial nuvoton file system */
    sysSetTimerReferenceClock (TIMER0, u32ExtFreq*1000);
    sysStartTimer(TIMER0, 100, PERIODIC_MODE);
    fsInitFileSystem();
    fsAssignDriveNumber('C', DISK_TYPE_SMART_MEDIA, 0, 1);     /* NAND 0, 2 partitions */
    fsAssignDriveNumber('D', DISK_TYPE_SMART_MEDIA, 0, 2);     /* NAND 0, 2 partitions */

    if(GNAND_InitNAND(&_nandDiskDriver0, &MassNDisk, TRUE) < 0)
    {
        sysprintf("GNAND_InitNAND error\n");
        return 0;
    }

    if(GNAND_MountNandDisk(&MassNDisk) < 0) 
    {
        sysprintf("GNAND_MountNandDisk error\n");
        return 0;
    }

    fsSetVolumeLabel('C', "NAND1-1\n", strlen("NAND1-1"));
    fsSetVolumeLabel('D', "NAND1-2\n", strlen("NAND1-2"));

    u32TotalSize = MassNDisk.nZone* MassNDisk.nLBPerZone*MassNDisk.nPagePerBlock*MassNDisk.nPageSize;
    /* Format NAND if necessery */
    if ((fsDiskFreeSpace('C', &block_size, &free_size, &disk_size) < 0) || 
        (fsDiskFreeSpace('D', &block_size, &free_size, &disk_size) < 0))
    {   
        if (fsTwoPartAndFormatAll((PDISK_T *)MassNDisk.pDisk, 32*1024, (u32TotalSize- 32*1024)) < 0)
        {
            sysprintf("Format failed\n");
            fsSetVolumeLabel('C', "NAND1-1\n", strlen("NAND1-1"));
            fsSetVolumeLabel('D', "NAND1-2\n", strlen("NAND1-2"));
            return 0;
        }
    }
#endif
#ifdef __SD__
    #ifdef  __SD_PORT0__
    sicIoctl(SIC_SET_CARD_DETECT, TRUE, 0, 0);  /* MUST call sicIoctl() BEFORE sicSdOpen0() */
    status = sicSdOpen0();
    if(status < 0)
        sicSdClose0();
    #elif defined (__SD_PORT1__)
    status = sicSdOpen1();
    if(status < 0)
        sicSdClose1();
    #else
    status = sicSdOpen2();
    if(status < 0)
        sicSdClose2();
    #endif
#endif
#endif

    mscdInit();

#ifdef __NAND_ONLY__
    mscdFlashInit(&MassNDisk,NULL);
#else
    #ifdef __SPI_ONLY__
    {
        UINT32 block_size, free_size, disk_size, reserved_size;

        sysSetTimerReferenceClock (TIMER0, u32ExtFreq*1000);
        sysStartTimer(TIMER0, 100, PERIODIC_MODE);

        fsInitFileSystem();
        fsAssignDriveNumber('C', DISK_TYPE_SD_MMC, 0, 1);

        reserved_size = 64*1024;        /* SPI reserved size before FAT = 64K */
        status = SpiFlashOpen(reserved_size);

        if (fsDiskFreeSpace('C', &block_size, &free_size, &disk_size) < 0)  
        {
            UINT32 u32BlockSize, u32FreeSize, u32DiskSize;
            PDISK_T  *pDiskList;

            //printf("Total SPI size = %d KB\n", u32TotalSectorSize/2);

            fsSetReservedArea(reserved_size/512);
            pDiskList = fsGetFullDiskInfomation();
            fsFormatFlashMemoryCard(pDiskList);
            fsReleaseDiskInformation(pDiskList);
            fsDiskFreeSpace('C', &u32BlockSize, &u32FreeSize, &u32DiskSize);   
            sysprintf("block_size = %d\n", u32BlockSize);
            sysprintf("free_size = %d\n", u32FreeSize);
            sysprintf("disk_size = %d\n", u32DiskSize);
        }
    }

    mscdFlashInit(NULL,status);
#elif defined (__RAM_DISK_ONLY__)
    mscdFlashInit(NULL,NULL);
#elif defined (__SD_ONLY__)
    /* Calling mscdSdPortSelect() with parameter - port index:0/1/2 can change the SD port used by MSC library before mscdFlashInit */
    /* Note: Corresponding SIC SD open - sicSdOpen0/sicSdOpen1/sicSdOpen2 must be called and set return value to the variable - status */
    mscdFlashInit(NULL,status);
#else
    /* Calling mscdSdPortSelect() with parameter - port index:0/1/2 can change the SD port used by MSC library before mscdFlashInit */
    /* Note: Corresponding SIC SD open - sicSdOpen0/sicSdOpen1/sicSdOpen2 must be called and set return value to the variable - status */
    mscdFlashInit(&MassNDisk,status);
#endif
#endif

#ifdef SUSPEND_POWERDOWN
    udcSetSupendCallBack(Demo_PowerDownWakeUp);
#endif

    udcInit();
#ifdef NON_BLOCK_MODE
    mscdBlcokModeEnable(FALSE);    /* Non-Block mode */
    while(1)
    {
        if(!PlugDetection())
            break;
        mscdMassEvent(NULL);
    }
#else
    mscdMassEvent(PlugDetection); /* Default : Block mode */
#endif

    mscdDeinit();
    udcDeinit();
    udcClose();
    sysprintf("Sample code End\n");
}



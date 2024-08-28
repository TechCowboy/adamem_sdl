/** Fujinet Device Emulation ************************************************/
/**                                                                        **/
/**                                fujinet.c                               **/
/**                                                                        **/
/** This file contains the Fujinet-specific emulation code                 **/
/**                                                                        **/
/** Copyleft Norman Davie 2024                                             **/
/**     You are free to distribute this one source file                    **/
/**     Please, notify me, if you make any changes to this file            **/
/****************************************************************************/

#include "../Coleco.h"
#include "fujinet.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#if defined(WIN32) || defined(MSDOS)
#if defined(IDEHD)
#include "../IDE/HarddiskIDE.h"
#endif
#else
#if defined(IDEHD)
#include "../IDE/HarddiskIDE.h"
#endif
#endif

#ifdef ZLIB
#include <zlib.h>
#endif

bool fujinet_verbose = true;


unsigned int Z80AddrFromDCBBuffer(unsigned dcb)
{
    DCB *Dcb;

    Dcb = (DCB *)&RAM[dcb];

    return (unsigned int)(Dcb->buf[1] & 0xFF) << 8 | (unsigned int)(Dcb->buf[0] & 0xFF);
}

void DisplayDCB(char *device, int mode, int device_id, unsigned dcb)
{
    DCB *Dcb;
    unsigned int z80_addr;

    Dcb = (DCB *) &RAM[dcb];
    
    if (! fujinet_verbose)
        return;

    printf("************************%s*****************************\n", device);
    // both the status and command byte. You write the command, the Adam does the operation, and writes back a status sometime later.
    printf("dcb address: $%04x  device id: %02x mode: %d\ncmd/status: %02x - ", (unsigned int) dcb, device_id, mode, (int)(Dcb->status) & 0xFF);

    switch (Dcb->status)
    {
    case DCB_CMD_STATUS:
        printf("STATUS");
        break;
    case DCB_CMD_SOFT_RESET:
        printf("SOFT RESET");
        break;
    case DCB_CMD_WRITE:
        printf("WRITE");
        break;
    case DCB_CMD_READ:
        printf("READ");
        break;
    default:
        break;
    }
    printf("\n");


    // 16 bit memory address in Z80 space for the target operation
    z80_addr = (unsigned int)(Dcb->buf[1] & 0xFF) << 8 | (unsigned int)(Dcb->buf[0] & 0xFF);
    printf("buf: %04x\n", z80_addr);

    if (z80_addr != 0)
    {
        for (int i = 0; i < Dcb->len; i++)
        {       
            printf("%02x ", RAM[z80_addr+i]);          
        }
        printf("\n");
    }
    // Length of I/O operation relative to the buffer.
    printf("len: %d\n", (int) Dcb->len);

    // Used by block devices to denote the desired 32-bit block address.
    printf("block: %ld\n", Dcb->block);

    // logical unit number specified.
    printf("local unit: %02x\n", (int) (Dcb->unit) & 0xFF);

    // The Adamnet node number for this device. (1 = keyboard, 2 = printer, etc.)
    printf("dev: %02x\n", (int) (Dcb->dev) & 0xFF);

    // The maximum size of payload allowed for this device
    printf("max_len: %ld\n", (int) Dcb->max_len);

    // Device type, 0 = character, 1 = block
    printf("type: %d\n", (int) (Dcb->type) & 0xFF);

    // The 8-bit status byte returned in the last byte of a status packet.
    printf("dev_status: %02x\n", (int) (Dcb->dev_status) & 0xFF);


    printf("\n");
    printf("*****************************************************\n");
}

void UpdateFujiDevice(int mode, int device_id, unsigned dcb)
{
    static int last_cmd = 0;
    unsigned int z80_addr;
    DCB *Dcb;
    int print_dcb = 1;


    Dcb = (DCB *) &RAM[dcb];

    switch(Dcb->status)
    {
    case 0xFF:
        Dcb->status = ACK;
        break;
    case DCB_CMD_STATUS:
        Dcb->status = ACK;
        break;
    case DCB_CMD_SOFT_RESET:
        Dcb->status = ACK;
        break;
    case DCB_CMD_WRITE:
        z80_addr = Z80AddrFromDCBBuffer(dcb);
        
        switch(RAM[z80_addr] & 0xFF)
        {
            case FUJINET_DEV_CMD_GET_TIME:
                last_cmd = RAM[z80_addr] & 0xFF;
                UpdateFujiClock(mode, device_id, dcb);
                print_dcb = 0;
                break;
            case FUJINET_DEV_CMD_RANDOM:
                last_cmd = RAM[z80_addr] & 0xFF;
                UpdateFujiRandom(mode, device_id, dcb);
                print_dcb = 0;
                break;
            case FUJINET_DEV_CMD_OPEN_APP_KEY:
                last_cmd = RAM[z80_addr] & 0xFF;
                UpdateFujiAppOpenKey(mode, device_id, dcb);
                print_dcb=0;
                break;
            case FUJINET_DEV_CMD_READ_APP_KEY:
                last_cmd = RAM[z80_addr] & 0xFF;
                UpdateFujiAppReadKey(mode, device_id, dcb);
                print_dcb = 0;
                break;
            case FUJINET_DEV_CMD_WRITE_APP_KEY:
                last_cmd = RAM[z80_addr] & 0xFF;
                UpdateFujiAppWriteKey(mode, device_id, dcb);
                print_dcb = 0;
                break;
            case FUJINET_DEV_CMD_CLOSE_APP_KEY:
                last_cmd = RAM[z80_addr] & 0xFF;
                UpdateFujiAppCloseKey(mode, device_id, dcb);
                print_dcb = 0;
                break;

            default:
                break;
        }
        
        break;
    case DCB_CMD_READ:

        z80_addr = Z80AddrFromDCBBuffer(dcb);
        switch (last_cmd)
        {
        case FUJINET_DEV_CMD_GET_TIME:
            UpdateFujiClock(mode, device_id, dcb);
            print_dcb = 0;
            break;
        case FUJINET_DEV_CMD_RANDOM:
            UpdateFujiRandom(mode, device_id, dcb);
            print_dcb = 0;
            break;
        case FUJINET_DEV_CMD_OPEN_APP_KEY:
            UpdateFujiAppOpenKey(mode, device_id, dcb);
            print_dcb = 0;
            break;
        case FUJINET_DEV_CMD_READ_APP_KEY:
            UpdateFujiAppReadKey(mode, device_id, dcb);
            print_dcb = 0;
            break;
        case FUJINET_DEV_CMD_WRITE_APP_KEY:
            UpdateFujiAppWriteKey(mode, device_id, dcb);
            print_dcb = 0;
            break;
        case FUJINET_DEV_CMD_CLOSE_APP_KEY:
            UpdateFujiAppCloseKey(mode, device_id, dcb);
            print_dcb = 0;
            break;
        default:
            break;
        }
        break;

    default:
        if (fujinet_verbose)
            printf("*******FUJIDEV: mode %02x | device id %02x | status %02x\n", mode, device_id, (int) (RAM[dcb]) & 0xFF);
        print_dcb = 0;
        Dcb->status = ACK;
        break;
    }

    if (Dcb->status != 0xFF)
        if (print_dcb)
            DisplayDCB("FUJIDEV", mode, device_id, dcb);
            
}







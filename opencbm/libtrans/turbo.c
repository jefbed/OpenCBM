#ifdef SAVE_RCSID
static char *rcsid =
    "@(#) $Id: turbo.c,v 1.3 2006-05-23 12:01:05 wmsr Exp $";
#endif

#include "libtrans.h"
#include "libtrans_int.h"

#include <stdio.h>

#if _MSC_VER >= 1400
    /* as long as we did not implement arch dependent secure
     * implementations of standard libc functions
     */
#   pragma warning( disable : 4996 )
#endif

static const unsigned char turbomain_drive_prog[] = {
#include "turbomain.inc"
};


#ifdef LIBOCT_STATE_DEBUG
volatile signed int stDebugLibOCTLineNumber = -1, stDebugLibOCTBlockCount = -1,
                    stDebugLibOCTByteCount  = -1, stDebugLibOCTBitCount   = -1;
volatile char *     stDebugLibOCTFileName   = "";

void
libopencbmtransfer_printStateDebugCounters(FILE *channel)
{
    fprintf(channel, "file: %s"
                     "\n\tversion: " OPENCBM_VERSION ", built: " __DATE__ " " __TIME__
                     "\n\tline=%d, blocks=%d, bytes=%d, bits=%d\n",
                     stDebugLibOCTFileName,   stDebugLibOCTLineNumber,
                     stDebugLibOCTBlockCount, stDebugLibOCTByteCount,
                     stDebugLibOCTBitCount);
}
#endif

/*
// functions to perform:

libopencbmtransfer_test()
*/

static transfer_funcs *current_transfer_funcs = &libopencbmtransfer_pp;

int
libopencbmtransfer_set_transfer(opencbm_transfer_t TransferType)
{
    switch (TransferType)
    {
    case opencbm_transfer_serial1:
        current_transfer_funcs = &libopencbmtransfer_s1;
        break;

    case opencbm_transfer_serial2:
        current_transfer_funcs = &libopencbmtransfer_s2;
        break;

    case opencbm_transfer_parallel:
        current_transfer_funcs = &libopencbmtransfer_pp;
        break;

    default:
        printf("Unknown transfer type %u!\n", TransferType);
        return 1;
    }

    return 0;
}


/*! \brief Install the turbo routines into a drive

 This functions installs the turbo routines for later
 use in the drive.

 \param HandleDevice  
   A CBM_FILE which contains the file handle of the driver.

 \param DeviceAddress
   The address of the device on the IEC serial bus. This
   is known as primary address, too.

 \return 
   0 means turbo routines have been installed successfully.
   Every other value denotes an error

 If this function does not return successfully, it is illegal
 to call any other libopencbmtransfer function.
*/
int
libopencbmtransfer_install(CBM_FILE HandleDevice, __u_char DeviceAddress)
{
    enum cbm_device_type_e cbmDeviceType;
    const char *cbmDeviceString;
    int error = 0;

    FUNC_ENTER();

    if (cbm_identify(HandleDevice, DeviceAddress, &cbmDeviceType, &cbmDeviceString))
    {
        error = 1;
        DBG_ERROR((DBG_PREFIX "cbm_identify returned with an error."));
    }
    else
    {
        DBG_SUCCESS((DBG_PREFIX "cbm_identify returned %s", cbmDeviceString));
    }


    // Upload turbo routines into drive

    current_transfer_funcs->upload(HandleDevice, DeviceAddress);

    if (!error)
    {
        int bytesWritten;

        // Now, upload the main loop into the drive

        bytesWritten = cbm_upload(HandleDevice, DeviceAddress, 0x500, 
            turbomain_drive_prog, sizeof(turbomain_drive_prog));

        if (bytesWritten != sizeof(turbomain_drive_prog))
        {
            DBG_ERROR((DBG_PREFIX "wanted to write %u bytes, but only %u "
                "bytes could be written" ,sizeof(turbomain_drive_prog), bytesWritten));

            error = 1;
        }
    }

    if (!error)
    {
        if (cbm_exec_command(HandleDevice, DeviceAddress, "U3:", 0))
        {
            DBG_ERROR((DBG_PREFIX "cbm_exec_command returnd with an error."));
            error = 1;
        }

        //printf("wait...\n");
        current_transfer_funcs->init(HandleDevice, DeviceAddress);
        //printf("... end\n");
    }

    FUNC_LEAVE_INT(error);
}


int
libopencbmtransfer_execute_command(CBM_FILE HandleDevice, __u_char DeviceAddress,
                                   unsigned int ExecutionAddress)
{
    current_transfer_funcs->write1byte(HandleDevice, 0x80);
    current_transfer_funcs->write2byte(HandleDevice, 
        (__u_char) (ExecutionAddress & 0xFF), 
        (__u_char) (ExecutionAddress >> 8));

    return 0;
}

typedef int
(*ll_read_write_mem)(CBM_FILE HandleDevice, __u_char DeviceAddress,
                     __u_char Buffer[], unsigned int MemoryAddress, unsigned int Length);

static int
libopencbmtransfer_ll_write_mem(CBM_FILE HandleDevice, __u_char DeviceAddress,
                                __u_char Buffer[], unsigned int MemoryAddress, unsigned int Length)
{
    FUNC_ENTER();

    DBG_ASSERT(Length < 0x100);

    current_transfer_funcs->write1byte(HandleDevice, 0x00);
    current_transfer_funcs->write2byte(HandleDevice,
        (__u_char) (MemoryAddress & 0xFF),
        (__u_char) (MemoryAddress >> 8));
    current_transfer_funcs->write1byte(HandleDevice, (__u_char) Length);
    current_transfer_funcs->writeblock(HandleDevice, Buffer, Length);

    FUNC_LEAVE_INT(0);
}


static int
libopencbmtransfer_ll_read_mem(CBM_FILE HandleDevice, __u_char DeviceAddress,
                               __u_char Buffer[], unsigned int MemoryAddress, unsigned int Length)
{
    FUNC_ENTER();

    DBG_ASSERT(Length < 0x100);

    current_transfer_funcs->write1byte(HandleDevice, 0x01);
    current_transfer_funcs->write2byte(HandleDevice,
        (__u_char) (MemoryAddress & 0xFF),
        (__u_char) (MemoryAddress >> 8));
    current_transfer_funcs->write1byte(HandleDevice, (__u_char) Length);
    current_transfer_funcs->readblock(HandleDevice, Buffer, Length);

    FUNC_LEAVE_INT(0);
}

static int
libopencbmtransfer_read_write_mem(CBM_FILE HandleDevice, __u_char DeviceAddress,
                                  __u_char Buffer[], unsigned int MemoryAddress, unsigned int Length,
                                  ll_read_write_mem function)
{
    FUNC_ENTER();

    // If we have to transfer more than one page, process the complete pages first
                                                                        SETSTATEDEBUG(stDebugLibOCTBlockCount = 0);
    while (Length >= 0x100)
    {
        char *statstr;
                                                                        SETSTATEDEBUG(stDebugLibOCTBlockCount++);
        //fprintf(stderr, "+"); fflush(stderr);
        switch ( (Length >> 8) & 3 )
        {
            case 0:  statstr = "\010.-"; break;
            case 1:  statstr = "\010/";  break;
            case 2:  statstr = "\010|";  break;
            default: statstr = "\010\\";
        }
        fprintf(stderr, "%s",statstr);
        fflush(stderr);

        function(HandleDevice, DeviceAddress, Buffer, MemoryAddress, 0x00);

        Buffer += 0x100;
        MemoryAddress += 0x100;
        Length -= 0x100;
    }

    if (Length > 0)
    {
        unsigned int remainder = 0x100 - Length;
                                                                        SETSTATEDEBUG(stDebugLibOCTBlockCount++);
        //fprintf(stderr, "."); fflush(stderr);
        fprintf(stderr, "\010.");
        fflush(stderr);
        function(HandleDevice, DeviceAddress, Buffer, MemoryAddress - remainder, remainder);
    }
                                                                        SETSTATEDEBUG(stDebugLibOCTBlockCount = -1);
    fprintf(stderr, "\010.\n");  // fflush(stderr);

    FUNC_LEAVE_INT(0);
}

int
libopencbmtransfer_read_mem(CBM_FILE HandleDevice, __u_char DeviceAddress,
                            __u_char Buffer[], unsigned int MemoryAddress, unsigned int Length)
{
    return libopencbmtransfer_read_write_mem(HandleDevice, DeviceAddress,
                                  Buffer, MemoryAddress, Length, libopencbmtransfer_ll_read_mem);
}

int
libopencbmtransfer_write_mem(CBM_FILE HandleDevice, __u_char DeviceAddress,
                            __u_char Buffer[], unsigned int MemoryAddress, unsigned int Length)
{
    return libopencbmtransfer_read_write_mem(HandleDevice, DeviceAddress,
                                  Buffer, MemoryAddress, Length, libopencbmtransfer_ll_write_mem);
}

int
libopencbmtransfer_remove(CBM_FILE HandleDevice, __u_char DeviceAddress)
{
    return libopencbmtransfer_execute_command(HandleDevice, DeviceAddress, 0xEBE7);
}

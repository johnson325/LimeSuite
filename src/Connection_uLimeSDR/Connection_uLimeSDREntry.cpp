/**
    @file Connection_uLimeSDREntry.cpp
    @author Lime Microsystems
    @brief Implementation of STREAM board connection.
*/

#include "Connection_uLimeSDR.h"
//#include "ConnectionSTREAM.h"
using namespace lime;

#ifdef __unix__
void Connection_uLimeSDREntry::handle_libusb_events()
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 250000;
    while(mProcessUSBEvents.load() == true)
    {
        int r = libusb_handle_events_timeout_completed(ctx, &tv, NULL);
        if(r != 0) printf("error libusb_handle_events %s\n", libusb_strerror(libusb_error(r)));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
#endif // __UNIX__

int Connection_uLimeSDR::USBTransferContext::idCounter=0;

//! make a static-initialized entry in the registry
void __loadConnection_uLimeSDREntry(void) //TODO fixme replace with LoadLibrary/dlopen
{
static Connection_uLimeSDREntry uLimeSDREntry;
}

Connection_uLimeSDREntry::Connection_uLimeSDREntry(void):
    ConnectionRegistryEntry("uLimeSDR")
{
#ifndef __unix__
    //m_pDriver = new CDriverInterface();
#else
    int r = libusb_init(&ctx); //initialize the library for the session we just declared
    if(r < 0)
        printf("Init Error %i\n", r); //there was an error
    libusb_set_debug(ctx, 3); //set verbosity level to 3, as suggested in the documentation
    mProcessUSBEvents.store(true);
    mUSBProcessingThread = std::thread(&Connection_uLimeSDREntry::handle_libusb_events, this);
#endif
}

Connection_uLimeSDREntry::~Connection_uLimeSDREntry(void)
{
#ifndef __unix__
    //delete m_pDriver;
#else
    mProcessUSBEvents.store(false);
    mUSBProcessingThread.join();
    libusb_exit(ctx);
#endif
}

std::vector<ConnectionHandle> Connection_uLimeSDREntry::enumerate(const ConnectionHandle &hint)
{
    std::vector<ConnectionHandle> handles;

#ifndef __unix__
    DWORD devCount = 0;
    FT_STATUS ftStatus = FT_OK;
    ftStatus = FT_ListDevices(&devCount, NULL, FT_LIST_NUMBER_ONLY);
    if(FT_FAILED(ftStatus))
        return handles;
    if (devCount > 0)
    {
        for(int i = 0; i<devCount; ++i)
        {
            ConnectionHandle handle;
            handle.media = "USB";
            handle.name = "uLimeSDR";
            handle.index = i;
            handles.push_back(handle);
        }
    }
#else
    libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
    int usbDeviceCount = libusb_get_device_list(ctx, &devs);
    if(usbDeviceCount > 0)
    {
        libusb_device_descriptor desc;
        for(int i=0; i<usbDeviceCount; ++i)
        {
            int r = libusb_get_device_descriptor(devs[i], &desc);
            if(r<0)
                printf("failed to get device description\n");
            int pid = desc.idProduct;
            int vid = desc.idVendor;

            if( vid == 0x0403)
            {
		if(pid == 0x601F)
                {
                    libusb_device_handle *tempDev_handle;
                    tempDev_handle = libusb_open_device_with_vid_pid(ctx, vid, pid);
                    if(libusb_kernel_driver_active(tempDev_handle, 0) == 1)   //find out if kernel driver is attached
                    {
                        if(libusb_detach_kernel_driver(tempDev_handle, 0) == 0) //detach it
                            printf("Kernel Driver Detached!\n");
                    }
                    if(libusb_claim_interface(tempDev_handle, 0) < 0) //claim interface 0 (the first) of device
                    {
                        printf("Cannot Claim Interface\n");
                    }

                    std::string fullName;
                    //check operating speed
                    int speed = libusb_get_device_speed(devs[i]);
                    if(speed == LIBUSB_SPEED_HIGH)
                        fullName = "USB 2.0";
                    else if(speed == LIBUSB_SPEED_SUPER)
                        fullName = "USB 3.0";
                    else
                        fullName = "USB";
                    fullName += " (";
                    //read device name
                    char data[255];
                    memset(data, 0, 255);
                    int st = libusb_get_string_descriptor_ascii(tempDev_handle, 2, (unsigned char*)data, 255);
                    if(strlen(data) > 0)
                        fullName += data;
                    fullName += ")";
                    libusb_close(tempDev_handle);

                    ConnectionHandle handle;
                    handle.media = "USB";
                    handle.name = fullName;
                    handle.addr = std::to_string(int(pid))+":"+std::to_string(int(vid));
                    handles.push_back(handle);
                }
            }
        }
    }
    else
    {
        libusb_free_device_list(devs, 1);
    }
#endif
    return handles;
}

IConnection *Connection_uLimeSDREntry::make(const ConnectionHandle &handle)
{
#ifndef __unix__
    return new Connection_uLimeSDR(mFTHandle, handle.index);
#else
    const auto pidvid = handle.addr;
    const auto splitPos = pidvid.find(":");
    const auto pid = std::stoi(pidvid.substr(0, splitPos));
    const auto vid = std::stoi(pidvid.substr(splitPos+1));
    return new Connection_uLimeSDR(ctx, handle.index, vid, pid);
    //return new ConnectionSTREAM(ctx, handle.index, vid, pid);
#endif
}

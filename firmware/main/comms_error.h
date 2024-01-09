#ifndef _COMMS_ERROR_H_
#define _COMMS_ERROR_H_

enum
{
    COMMS_OK                        = 0,
    // Configure errors
    COMMS_ERROR_MISSING_PARAMETER   = -1,
    // Connect errors
    COMMS_ERROR_LTE_NOT_CONFIGURED  = -2,
    COMMS_ERROR_WIFI_NOT_CONFIGURED = -3,
    COMMS_ERROR_CONNECT_FAILED      = -4,
    COMMS_OFFLINE                   = -5,
    COMMS_BACKEND_GONE_MISSING      = -6,
    // User errors
    COMMS_NOT_IMPLEMENTED           = -7, //...
    COMMS_BUFFER_OVERRUN            = -8,
};


#endif // _COMMS_ERROR_H_
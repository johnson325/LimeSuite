#include "IConnection.h"
#include "ConnectionRegistry.h"
#include "lms_gfir.h"
#include "lms7_api.h"
#include "lms7_device.h"
#include "ErrorReporting.h"
#include "errno.h"
#include "MCU_BD.h"
#include <cmath>


using namespace std;


API_EXPORT int CALL_CONV LMS_GetDeviceList(lms_info_str_t * dev_list)
{
    std::vector<lime::ConnectionHandle> handles;
    handles = lime::ConnectionRegistry::findConnections();
  
    if (dev_list != nullptr)
    {
        for (int i = 0; i < handles.size(); i++)
        {
            string str = handles[i].serialize();
            if (dev_list[i] == nullptr)
                return -1;
            strncpy(dev_list[i],str.c_str(),sizeof(lms_info_str_t)-1);
            dev_list[i][sizeof(lms_info_str_t)-1]=0;
        }  
    } 
    return handles.size();
}




API_EXPORT int CALL_CONV LMS_Open(lms_device_t** device, lms_info_str_t info)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device pointer cannot be NULL");
        return -1;
    }
     
    std::vector<lime::ConnectionHandle> handles;
    handles = lime::ConnectionRegistry::findConnections();
    LMS7_Device* lms;
    if (*device == nullptr)
    {
        lms = new LMS7_Device();
        *device = lms;
    }
    else
    {
        lms = (LMS7_Device*)*device;
        auto conn = lms->GetConnection();
        if (conn != nullptr)
		lime::ConnectionRegistry::freeConnection(conn);
    }

    for (int i = 0; i < handles.size(); i++)
    {
        if (info == NULL || strcmp(handles[i].serialize().c_str(),info) == 0)
        {
            auto conn = lime::ConnectionRegistry::makeConnection(handles[i]);
            if (conn->IsOpen() == false)
            {
                if (info != NULL)
                {
                    lime::ReportError(EBUSY, "Failed to open. Device is busy.");
                    delete lms;
                    return -1;
                }
                else 
                    continue;
            }
            lms->SetConnection(conn,0);
            lms->DownloadAll();
            return LMS_SUCCESS;
        }
    }  
    
    if (info == NULL)
       return LMS_SUCCESS;
    
    lime::ReportError(ENODEV, "Specified device could not be found");
    delete lms;
    return -1;    
}

API_EXPORT int CALL_CONV LMS_Close(lms_device_t * device)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
     
    LMS7_Device* lms = (LMS7_Device*)device;
    auto conn = lms->GetConnection();
    delete lms;
    if (conn != nullptr)
    {
		lime::ConnectionRegistry::freeConnection(conn);
    }
    return LMS_SUCCESS;
}

API_EXPORT int CALL_CONV LMS_Reset(lms_device_t *device)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }

    LMS7_Device* lms = (LMS7_Device*)device;
    if (lms->ResetChip()!=0)
        return -1;
    lms->DownloadAll();
    return LMS_SUCCESS;   
}

API_EXPORT int CALL_CONV LMS_EnableChannel(lms_device_t * device, bool dir_tx, size_t chan, bool enabled)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    if (dir_tx)
    {
       if (lms->EnableTX(chan,enabled)!=0)
           return -1;
    }
    else
    {
       if (lms->EnableRX(chan,enabled)!=0)
           return -1;
    }
    return LMS_SUCCESS;
}



API_EXPORT int CALL_CONV LMS_SetSampleRate(lms_device_t * device, float_type rate, size_t oversample)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
   LMS7_Device* lms = (LMS7_Device*)device; 
   
   return lms->SetRate(rate, oversample);
}

API_EXPORT int CALL_CONV LMS_SetSampleRateDir(lms_device_t *device, bool dir_tx, float_type rate, size_t oversample)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    return lms->SetRate(dir_tx,rate,oversample);   
}


API_EXPORT int CALL_CONV LMS_GetSampleRate(lms_device_t *device, bool dir_tx, size_t chan, float_type *host_Hz, float_type *rf_Hz)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
   
    LMS7_Device* lms = (LMS7_Device*)device; 
      
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    double rate = lms->GetRate(dir_tx,chan,rf_Hz);
    if (host_Hz)
        *host_Hz = rate;

    return LMS_SUCCESS;
   
}

API_EXPORT int CALL_CONV LMS_GetSampleRateRange(lms_device_t *device, bool dir_tx, lms_range_t *range)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    if (dir_tx)
        *range = lms->GetRxRateRange();
    else
        *range = lms->GetTxRateRange();
    return LMS_SUCCESS; 
}



API_EXPORT int CALL_CONV LMS_Init(lms_device_t * device)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    return lms->Init(); 
}

API_EXPORT int CALL_CONV LMS_SetReferenceClock(lms_device_t *device, float_type clock_Hz)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    return lms->SetReferenceClock(clock_Hz);
}

API_EXPORT int CALL_CONV LMS_GetReferenceClock(lms_device_t * device, float_type * clock_Hz)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    *clock_Hz = lms->GetReferenceClk_SX(false);
    return LMS_SUCCESS;
}

API_EXPORT int CALL_CONV LMS_ReadCustomBoardParam(lms_device_t *device,
                           uint16_t param_id, float_type *val, lms_name_t units)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    LMS7_Device* lms = (LMS7_Device*)device; 
    uint8_t id=param_id;
    std::string str;
    int ret=lms->GetConnection()->CustomParameterRead(&id,val,1,&str);
    strncpy(units,str.c_str(),sizeof(lms_name_t)-1);
    return ret;
}

API_EXPORT int CALL_CONV LMS_WriteCustomBoardParam(lms_device_t *device,
                        uint16_t param_id, float_type val, const lms_name_t units)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device; 
    uint8_t id=param_id;
    std::string str = units == nullptr ? "":units;
    return lms->GetConnection()->CustomParameterWrite(&id,&val,1,&str);
}

API_EXPORT int CALL_CONV LMS_VCTCXOWrite(lms_device_t * device, uint16_t val)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device; 
    return lms->DACWrite(val);
}

API_EXPORT int CALL_CONV LMS_VCTCXORead(lms_device_t * device, uint16_t *val)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device; 
    int ret = lms->DACRead();
    *val = ret;
    return ret < 0 ? -1 : 0;
}

API_EXPORT int CALL_CONV LMS_GetVCORange(lms_device_t * device, size_t vco_id, lms_range_t* range)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device; 
    if (vco_id < 3)
    {
        range->min=lms->gVCO_frequency_table[vco_id][0];
        range->max=lms->gVCO_frequency_table[vco_id][1];
    }
    else if (vco_id == 3)
    {
        range->min=lms->gCGEN_VCO_frequencies[0];
        range->max=lms->gCGEN_VCO_frequencies[1];
    }
    else
    {
        lime::ReportError(EINVAL, "VCO ID out of range.");
        return -1;
    }
    return 0;
}

API_EXPORT int CALL_CONV LMS_SetVCORange(lms_device_t * device, size_t vco_id, lms_range_t range)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device; 
    if (vco_id < 3)
    {
        lms->gVCO_frequency_table[vco_id][0]=range.min;
        lms->gVCO_frequency_table[vco_id][1]=range.max;
    }
    else if (vco_id == 3)
    {
        lms->gCGEN_VCO_frequencies[0]=range.min;
        lms->gCGEN_VCO_frequencies[1]=range.max;
    }
    else
    {
        lime::ReportError(EINVAL, "VCO ID out of range.");
        return -1;
    }
    return 0;
}

API_EXPORT int CALL_CONV LMS_TuneFilter(lms_device_t * device, size_t chan, lms_filter_t filt, const float_type *bw)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels())
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    if (lms->Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
    
    int ret;
    
    switch (filt)
    {
        case LMS_RX_LPF_TIA:
                ret = lms->TuneRxFilter(lime::LMS7002M::RxFilter::RX_TIA, *bw);
        case LMS_RX_LPF_LOWBAND: 
                ret = lms->TuneRxFilter(lime::LMS7002M::RxFilter::RX_LPF_LOWBAND, *bw);
        case LMS_RX_LPF_HIGHBAND:
                ret = lms->TuneRxFilter(lime::LMS7002M::RxFilter::RX_LPF_HIGHBAND, *bw);
        case LMS_TX_LPF_HIGHBAND:
                ret = lms->TuneTxFilter(lime::LMS7002M::TX_HIGHBAND,*bw);
        case LMS_TX_LPF_REALPOLE:
                ret = lms->TuneTxFilter(lime::LMS7002M::TX_REALPOLE,*bw);
        case LMS_TX_LPF_LADDER:
                ret = lms->TuneTxFilter(lime::LMS7002M::TX_LADDER,*bw);
        case LMS_TX_LPF_LOWCHAIN:
                ret = lms->TuneTxFilterLowBandChain(bw[0],bw[1]);
        default: 
                lime::ReportError(EINVAL, "Invalid filter parameter");
                return -1;
        
    }
    if (ret == 0)
        return lms->DownloadAll();
    
    return -1;
}

API_EXPORT int CALL_CONV LMS_GetClockFreq(lms_device_t *device, size_t clk_id, float_type *freq)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device;   
    
    switch(clk_id)
    {
        case LMS_CLOCK_REF:
            *freq = lms->GetReferenceClk_SX(lime::LMS7002M::Rx);
            return 0;
        case LMS_CLOCK_SXR:
            *freq = lms->GetFrequencySX(false);
            return 0;
        case LMS_CLOCK_SXT:
            *freq = lms->GetFrequencySX(true);
            return 0;
        case LMS_CLOCK_CGEN:
            *freq = lms->GetFrequencyCGEN();
            return 0;
        case LMS_CLOCK_RXTSP:
            *freq = lms->GetReferenceClk_TSP(false);
            return 0;
        case LMS_CLOCK_TXTSP:
            *freq = lms->GetReferenceClk_TSP(true);
            return 0;
        default:
            lime::ReportError(EINVAL, "Invalid clock ID.");
            return -1;
    }
}

API_EXPORT int CALL_CONV LMS_SetClockFreq(lms_device_t *device, size_t clk_id, float_type freq)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device;   
    
    switch (clk_id)
    {
        case LMS_CLOCK_REF:
            if (freq <= 0)
            {
                lime::ReportError(EINVAL, "Invalid frequency value.");
                return -1;
            }
            lms->SetReferenceClk_SX(lime::LMS7002M::Tx,freq);
            lms->SetReferenceClk_SX(lime::LMS7002M::Rx,freq);
            return 0;
        case LMS_CLOCK_SXR:
            if (freq <= 0)
                return lms->TuneVCO(lime::LMS7002M::VCO_SXR);
            return lms->SetFrequencySX(false,freq);
        case LMS_CLOCK_SXT:
            if (freq <= 0)
                return lms->TuneVCO(lime::LMS7002M::VCO_SXT);
            return lms->SetFrequencySX(true,freq);
        case LMS_CLOCK_CGEN:
        {
            int ret;
            float_type fpgaTxPLL = lms->GetReferenceClk_TSP(lime::LMS7002M::Tx) / pow(2.0, lms->Get_SPI_Reg_bits(LMS7param(HBI_OVR_TXTSP)));
            float_type fpgaRxPLL = lms->GetReferenceClk_TSP(lime::LMS7002M::Rx) / pow(2.0, lms->Get_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP)));
            if (freq <= 0)
            {
                ret = lms->TuneVCO(lime::LMS7002M::VCO_CGEN);
            }
            else
            {
                ret = lms->SetInterfaceFrequency(freq, lms->Get_SPI_Reg_bits(LMS7param(HBI_OVR_TXTSP)), lms->Get_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP)));
            }
            if (ret != 0)
                return -1;
            
            lms->GetConnection()->UpdateExternalDataRate(0,fpgaTxPLL/2,fpgaRxPLL/2);
            return 0;
        }
        case LMS_CLOCK_RXTSP:
                lime::ReportError(ENOTSUP, "Setting TSP clocks is not supported.");
                return -1;     
        case LMS_CLOCK_TXTSP:
                lime::ReportError(ENOTSUP, "Setting TSP clocks is not supported.");
                return -1;  
        default:
            lime::ReportError(EINVAL, "Invalid clock ID.");
            return -1;
    }
}



API_EXPORT int CALL_CONV LMS_GetNumChannels(lms_device_t * device, bool dir_tx)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    return lms->GetNumChannels(dir_tx);
}


API_EXPORT int CALL_CONV LMS_SetLOFrequency(lms_device_t *device, bool dir_tx, size_t chan, float_type frequency)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    if (chan >= lms->GetNumChannels(false))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    if (dir_tx)  
    {
        if (lms->SetTxFrequency(chan,frequency)!=0)
            return -1;
    }
    else
    {
        if (lms->SetRxFrequency(chan,frequency)!=0)
            return -1;
    }

    return LMS_SUCCESS;     
}


API_EXPORT int CALL_CONV LMS_GetLOFrequency(lms_device_t *device, bool dir_tx, size_t chan, float_type *frequency)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
     
    *frequency = lms->GetFrequencySX(dir_tx);
    return LMS_SUCCESS;  
}

API_EXPORT int CALL_CONV LMS_GetLOFrequencyRange(lms_device_t *device, bool dir_tx, lms_range_t *range)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    *range = lms->GetFrequencyRange(dir_tx);
    return LMS_SUCCESS; 
}

API_EXPORT int CALL_CONV LMS_GetAntennaList(lms_device_t *device, bool dir_tx, size_t chan, lms_name_t *list)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(false))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    auto names = lms->GetPathNames(dir_tx,chan);
    for (int i = 0; i<names.size();i++)
    {
      strncpy(list[i],names[i].c_str(),sizeof(lms_name_t)-1);  
      list[i][sizeof(lms_name_t)-1] = 0;
    }
    return names.size();  
}

API_EXPORT int CALL_CONV LMS_SetAntenna(lms_device_t *device, bool dir_tx, size_t chan, size_t path)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(false))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    lms->SetPath(dir_tx,chan,path);

    return LMS_SUCCESS;  
}


API_EXPORT int CALL_CONV LMS_GetAntenna(lms_device_t *device, bool dir_tx, size_t chan, size_t *path)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    *path = lms->GetPath(dir_tx, chan);  
    return *path < 0 ? -1 : LMS_SUCCESS;   
}



API_EXPORT int CALL_CONV LMS_GetAntennaBW(lms_device_t *device, bool dir_tx, size_t chan, size_t path, lms_range_t *range)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    if (dir_tx)
        *range = lms->GetTxPathBand(path,chan);
    else
        *range = lms->GetRxPathBand(path,chan);
    
    return LMS_SUCCESS;    
}


API_EXPORT int CALL_CONV LMS_SetBW(lms_device_t *device, bool dir_tx, size_t chan, float_type bandwidth)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->SetBandwidth(dir_tx,chan,bandwidth);
}


API_EXPORT int CALL_CONV LMS_GetBW(lms_device_t *device, bool dir_tx, size_t chan, float_type *bandwidth)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    *bandwidth = lms->GetBandwidth(dir_tx,chan);
    
    return LMS_SUCCESS;  
}


API_EXPORT int CALL_CONV LMS_GetBWRange(lms_device_t *device, bool dir_tx, lms_range_t *range)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    *range = lms->GetBandwidthRange(dir_tx);
    
    return LMS_SUCCESS;  
}


API_EXPORT int CALL_CONV LMS_SetLPFBW(lms_device_t *device, bool dir_tx, size_t chan, float_type bandwidth)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->SetLPF(dir_tx,chan,true,true,bandwidth);
}

API_EXPORT int CALL_CONV LMS_GetLPFBW(lms_device_t *device, bool dir_tx, size_t chan, float_type *bandwidth)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    *bandwidth = lms->GetLPFBW(dir_tx,chan,true);
    return LMS_SUCCESS;
}

API_EXPORT int CALL_CONV LMS_SetLPF(lms_device_t *device, bool dir_tx, size_t chan, bool enabled)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->SetLPF(dir_tx,chan,true,enabled,1);  
}


API_EXPORT int CALL_CONV LMS_SetGFIRLPF(lms_device_t *device, bool dir_tx, size_t chan, bool enabled, float_type bandwidth)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->SetLPF(dir_tx,chan,false,enabled,bandwidth);
}

API_EXPORT int CALL_CONV LMS_GetLPFBWRange(lms_device_t *device, bool dir_tx, lms_range_t *range)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    *range = lms->GetLPFRange(dir_tx,0,true);

    return LMS_SUCCESS; 
}


API_EXPORT int CALL_CONV LMS_SetNormalizedGain(lms_device_t *device, bool dir_tx, size_t chan, float_type gain)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    if (gain > 1.0)
        gain = 1.0;
    else if (gain < 0)
        gain = 0;
    
   return lms->SetNormalizedGain(dir_tx,chan,gain);
}

API_EXPORT int CALL_CONV LMS_GetNormalizedGain(lms_device_t *device, bool dir_tx, size_t chan,float_type *gain)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    *gain = lms->GetNormalizedGain(dir_tx,chan);
    if (gain < 0)
        return -1;
    return LMS_SUCCESS;   
}

API_EXPORT int CALL_CONV LMS_Calibrate(lms_device_t *device, bool dir_tx, size_t chan, double bw, unsigned flags)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    lms->EnableCalibrationByMCU(true);
    lms->Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true);
    if (dir_tx)
       return lms->CalibrateTx(bw,flags!=0);   
    else
       return lms->CalibrateRx(bw,flags!=0);

}

API_EXPORT int CALL_CONV LMS_LoadConfig(lms_device_t *device, const char *filename)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    return lms->LoadConfig(filename);
}

API_EXPORT int CALL_CONV LMS_SaveConfig(lms_device_t *device, const char *filename)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    return lms->SaveConfig(filename);  
}
API_EXPORT int CALL_CONV LMS_SetTestSignal(lms_device_t *device, bool dir_tx, size_t chan, lms_testsig_t sig, int16_t dc_i, int16_t dc_q)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;   
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    if (sig > LMS_TESTSIG_DC)
    {
        lime::ReportError(EINVAL, "Invalid signal.");
        return -1;
    }
    
    lms->SetTestSignal(dir_tx,chan,sig,dc_i,dc_q);
    
    return LMS_SUCCESS;  
}

API_EXPORT int CALL_CONV LMS_GetTestSignal(lms_device_t *device, bool dir_tx, size_t chan, lms_testsig_t *sig)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    int tmp = lms->GetTestSignal(dir_tx,chan);
    if (tmp < 0)
        return -1;
    
    *sig = (lms_testsig_t)tmp;  
    return LMS_SUCCESS;  
}

API_EXPORT int CALL_CONV LMS_SetNCOFrequency(lms_device_t *device, bool dir_tx, size_t ch, const float_type *freq, float_type pho)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (ch >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->SetNCOFreq(dir_tx, ch,freq, pho);
}


API_EXPORT int CALL_CONV LMS_GetNCOFrequency(lms_device_t *device, bool dir_tx, size_t chan, float_type *freq, float_type *pho)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->GetNCOFreq(dir_tx,chan,freq,pho);
}

API_EXPORT int CALL_CONV LMS_SetNCOPhase(lms_device_t *device, bool dir_tx, size_t ch, const float_type *phase, float_type fcw)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (ch >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->SetNCOPhase(dir_tx, ch,phase, fcw);
}


API_EXPORT int CALL_CONV LMS_GetNCOPhase(lms_device_t *device, bool dir_tx, size_t ch, size_t index, float_type *phase, float_type *fcw)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (ch >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->GetNCOPhase(dir_tx,ch,phase,fcw);
}

API_EXPORT int CALL_CONV LMS_SetNCOIndex(lms_device_t *device, bool dir_tx, size_t chan, size_t index,bool down)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->SetNCO(dir_tx,chan,index,down);
}

API_EXPORT int CALL_CONV LMS_GetNCOIndex(lms_device_t *device, bool dir_tx, size_t chan, size_t *index)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    if (chan >= lms->GetNumChannels(dir_tx))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    *index = lms->GetNCO(dir_tx,chan);
    if (*index < 0)
        return -1; 
    return LMS_SUCCESS;
}

API_EXPORT int CALL_CONV LMS_GenerateLPFCoef(size_t n, float_type w1, float_type w2, float_type g_stop, float_type *coef)
{
    if (w1 > 0.5 || w1 < 0 || w2 > 0.5 || w2 < 0)
    {
        lime::ReportError(EINVAL, "Normalized frequency (w1 or w2) out of range [0, 0.5]");
           return -1;
    }
    
    if (g_stop > 0.5 || g_stop < 0)
    {
        lime::ReportError(EINVAL, "stop band gain (g_stop )out of range [0, 0.5]");
           return -1;
    }
    
    float_type a1,a2;
    if (w1 < w2)
    {
        a1 = 1.0;
        a2 = g_stop;
    }
    else
    {
        a2 = 1.0;
        a1 = g_stop;
    }
   
    GenerateFilter(n, w1, w2, a1, a2, coef);
    return LMS_SUCCESS;
    
}

API_EXPORT int CALL_CONV LMS_ReadLMSReg(lms_device_t *device, uint32_t address, uint16_t *val)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    *val = lms->SPI_read(address,true);
    return LMS_SUCCESS;
}

API_EXPORT int CALL_CONV LMS_WriteLMSReg(lms_device_t *device, uint32_t address, uint16_t val)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    lms->SPI_write(address,val);
    return LMS_SUCCESS;  
}

API_EXPORT int CALL_CONV LMS_RegisterTest(lms_device_t *device)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;   
    return lms->RegistersTest();   
}


API_EXPORT int CALL_CONV LMS_ReadFPGAReg(lms_device_t *device, uint16_t address, uint16_t *val)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    uint32_t addr = address;
    uint32_t data;
    *val = lms->GetConnection()->ReadRegisters(&addr,&data,1);
    *val = data;
    return LMS_SUCCESS;
}

API_EXPORT int CALL_CONV LMS_WriteFPGAReg(lms_device_t *device, uint16_t address, uint16_t val)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    val = lms->GetConnection()->WriteRegister(address,val);
    return LMS_SUCCESS;  
}

API_EXPORT int CALL_CONV LMS_ReadParam(lms_device_t *device, struct LMS7Parameter param, uint16_t *val)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    *val = lms->Get_SPI_Reg_bits(param);
    return LMS_SUCCESS;
}



API_EXPORT int CALL_CONV LMS_WriteParam(lms_device_t *device, struct LMS7Parameter param, uint16_t val)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    return lms->Modify_SPI_Reg_bits(param,val);
}

API_EXPORT int CALL_CONV LMS_SetGFIRCoeff(lms_device_t * device, bool dir_tx, size_t chan, lms_gfir_t filt, const float_type* coef,size_t count)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    LMS7_Device* lms = (LMS7_Device*)device;
    
    if (chan >= lms->GetNumChannels(false))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    return lms->SetGFIRCoef(dir_tx,chan,filt,coef,count);
}

API_EXPORT int CALL_CONV LMS_GetGFIRCoeff(lms_device_t * device, bool dir_tx, size_t chan, lms_gfir_t filt, float_type* coef)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    LMS7_Device* lms = (LMS7_Device*)device;  
    
    if (chan >= lms->GetNumChannels(false))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    return lms->GetGFIRCoef(dir_tx,chan,filt,coef);
}
API_EXPORT int CALL_CONV LMS_SetGFIR(lms_device_t * device, bool dir_tx, size_t chan, lms_gfir_t filt, bool enabled)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    if (chan >= lms->GetNumChannels(false))
    {
        lime::ReportError(EINVAL, "Invalid channel number.");
        return -1;
    }
    
    return lms->SetGFIR(dir_tx,chan,filt,enabled);
}

API_EXPORT int CALL_CONV LMS_SetStreamingMode(lms_device_t *device, uint32_t flags)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    LMS7_Device* lms = (LMS7_Device*)device;  
    return lms->SetStreamingMode(flags);   
}

API_EXPORT int CALL_CONV LMS_InitStream(lms_device_t *device, bool tx, size_t num_buffers, size_t buffer_size, size_t fifo_size)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    if (tx)
        return lms->ConfigureTxStream(num_buffers,buffer_size,fifo_size);
    else
        return lms->ConfigureRxStream(num_buffers,buffer_size,fifo_size);
}

API_EXPORT int CALL_CONV LMS_StopStream(lms_device_t *device, bool dir_tx)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    if (dir_tx)
        return lms->StopTx();
    else
        return lms->StopRx();
}

API_EXPORT int CALL_CONV LMS_RecvStream(lms_device_t *device, void **samples, size_t sample_count, lms_stream_meta_t *meta, unsigned timeout_ms)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    return lms->RecvStream(samples,sample_count,meta,timeout_ms);
    
}
API_EXPORT int CALL_CONV LMS_SendStream(lms_device_t *device, const void **samples, size_t sample_count, lms_stream_meta_t *meta, unsigned timeout_ms)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    }
    
    LMS7_Device* lms = (LMS7_Device*)device;  
    return lms->SendStream(samples,sample_count,meta,timeout_ms);
}


API_EXPORT int CALL_CONV LMS_GetDeviceInfo(lms_device_t *device, lms_dev_info_t *info)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    
    LMS7_Device* lms = (LMS7_Device*)device;
    
    if (!lms->GetConnection()->IsOpen())
    {
       lime::ReportError(EINVAL, "No cennection to board.");
       return -1;
    }

    memset(info,0,sizeof(lms_dev_info_t));
    auto devinfo =lms->GetConnection()->GetDeviceInfo();
    strncpy(info->deviceName,devinfo.deviceName.c_str(),sizeof(info->deviceName)-1);
    strncpy(info->expansionName,devinfo.expansionName.c_str(),sizeof(info->expansionName)-1);
    strncpy(info->firmwareVersion,devinfo.firmwareVersion.c_str(),sizeof(info->firmwareVersion)-1);
    strncpy(info->hardwareVersion,devinfo.hardwareVersion.c_str(),sizeof(info->hardwareVersion)-1);
    strncpy(info->protocolVersion,devinfo.protocolVersion.c_str(),sizeof(info->protocolVersion)-1);
    info->boardSerialNumber = devinfo.boardSerialNumber;
    return 0;
}

API_EXPORT int CALL_CONV LMS_ProgramFPGA(lms_device_t *device, const char *data,
                                            size_t size, lms_target_t target)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    return lms->ProgramFPGA(data,size,target);  
}

API_EXPORT int CALL_CONV LMS_ProgramFPGAFile(lms_device_t *device,
                                        const char *file, lms_target_t target)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    std::string str = file;
    return lms->ProgramFPGA(str,target);  
}


API_EXPORT int CALL_CONV LMS_ProgramFirmware(lms_device_t *device, const char *data,
                                            size_t size, lms_target_t target)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    return lms->ProgramFW(data,size,target);   
}


API_EXPORT int CALL_CONV LMS_ProgramFirmwareFile(lms_device_t *device,
                                         const char *file, lms_target_t target)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    return lms->ProgramFW(file,target);   
}

API_EXPORT int CALL_CONV LMS_ProgramLMSMCU(lms_device_t *device, const char *data,
                                              size_t size, lms_target_t target)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    
    return lms->ProgramMCU(data,size,target);  
}

API_EXPORT int CALL_CONV LMS_ResetLMSMCU(lms_device_t *device)
{
    if (device == nullptr)
    {
        lime::ReportError(EINVAL, "Device cannot be NULL.");
        return -1;
    } 
    
    LMS7_Device* lms = (LMS7_Device*)device; 
    lime::MCU_BD mcu;
    mcu.Initialize(lms->GetConnection());
    mcu.Reset_MCU();
    return 0;
}


API_EXPORT int CALL_CONV LMS_GetLastError()
{
    return lime::GetLastError();
}

API_EXPORT const char * CALL_CONV LMS_GetLastErrorMessage(void)
{
    return lime::GetLastErrorMessage();
}

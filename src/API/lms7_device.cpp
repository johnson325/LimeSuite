/* 
 * File:   lms7_device.cpp
 * Author: ignas
 * 
 * Created on March 9, 2016, 12:54 PM
 */

#include "lms7_device.h"
#include "GFIR/lms_gfir.h"
#include "IConnection.h"
#include <cmath>
#include "dataTypes.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include "ErrorReporting.h"
#include "MCU_BD.h"
#include "FPGA_common.h"


static const size_t LMS_PATH_NONE = 0;
static const size_t LMS_PATH_LOW = 2;
static const size_t LMS_PATH_HIGH = 1;
static const size_t LMS_PATH_WIDE = 3;

const double LMS7_Device::LMS_CGEN_MAX = 640000000;

LMS7_Device::LMS7_Device() : LMS7002M(){
    
    tx_channels = new lms_channel_info[2];
    rx_channels = new lms_channel_info[2];
    tx_channels[0].enabled = true;
    rx_channels[0].enabled = true;
    tx_channels[1].enabled = false;
    rx_channels[1].enabled = false;
    rx_channels[0].half_duplex = false;
    rx_channels[1].half_duplex = false;
    
    streamer = nullptr;

    EnableValuesCache(false);
}

int LMS7_Device::ConfigureRXLPF(bool enabled,int ch,float_type bandwidth)
{

    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch-1,true)!=0)
        return -1;
    
    if (enabled)
    {
        if (Modify_SPI_Reg_bits(LMS7param(PD_LPFL_RBB),0,true)!=0)
            return -1;     
        Modify_SPI_Reg_bits(LMS7param(PD_LPFH_RBB),0,true);
        Modify_SPI_Reg_bits(LMS7param(INPUT_CTL_PGA_RBB),0,true);
        
        if (bandwidth > 0)
        {
            if (TuneRxFilter(bandwidth)!=0)
                return -1;
        }
    }
    else
    {
        if (Modify_SPI_Reg_bits(LMS7param(PD_LPFL_RBB),1,true)!=0)
            return -1;     
        Modify_SPI_Reg_bits(LMS7param(PD_LPFH_RBB),1,true);
        Modify_SPI_Reg_bits(LMS7param(INPUT_CTL_PGA_RBB),2,true);
              
    }

    return 0;
}


int LMS7_Device::ConfigureGFIR(bool enabled,bool tx, double bandwidth, size_t ch)
{
    double w,w2;
    int L;
    int div = 1;
    
    bandwidth /= 1e6;
   
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -1;
         
    if (tx)
    {
        Modify_SPI_Reg_bits(LMS7param(GFIR1_BYP_TXTSP),enabled==false,true);
        Modify_SPI_Reg_bits(LMS7param(GFIR2_BYP_TXTSP),enabled==false,true);
        Modify_SPI_Reg_bits(LMS7param(GFIR3_BYP_TXTSP),enabled==false,true);
                
    }
    else       
    {
        Modify_SPI_Reg_bits(LMS7param(GFIR1_BYP_RXTSP),enabled==false,true);
        Modify_SPI_Reg_bits(LMS7param(GFIR2_BYP_RXTSP),enabled==false,true);
        Modify_SPI_Reg_bits(LMS7param(GFIR3_BYP_RXTSP),enabled==false,true);
            
    }    
    
    if (bandwidth < 0)
        return 0;
    
    if (enabled)
    {
        
        double interface_MHz;
        int ratio;
        
        if (tx)
        {
          ratio = Get_SPI_Reg_bits(LMS7param(HBI_OVR_TXTSP));
          interface_MHz = GetReferenceClk_TSP(lime::LMS7002M::Tx)/1e6;
        }
        else
        {
          ratio =Get_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP));
          interface_MHz = GetReferenceClk_TSP(lime::LMS7002M::Rx)/1e6;
        }
        
        if (ratio == 7)
            interface_MHz /= 2;
        else
            div = (2<<(ratio));
        
        w = (bandwidth/2)/(interface_MHz/div);
        
        L = div > 8 ? 8 : div;
        div -= 1;
        
        w *=0.95;
        w2 = w*1.1;
        if (w2 > 0.495)
        {
         w2 = w*1.05;
         if (w2 > 0.495)  
         {
             return 0; //Filter disabled    
         }
        }
    }     
    else return 0;
  
  double coef[120];
  double coef2[40];
  short gfir1[120];
  short gfir2[40];

  GenerateFilter(L*15, w, w2, 1.0, 0, coef);
  GenerateFilter(L*5, w, w2, 1.0, 0, coef2);
  
    int sample = 0;
    for(int i=0; i<15; i++)
    {
	for(int j=0; j<8; j++)
        {
            if( (j < L) && (sample < L*15) )
            {
                gfir1[i*8+j] = (coef[sample]*32767.0);
		sample++;              
            }
            else 
            {
		gfir1[i*8+j] = 0;
            }
	}
    } 

    sample = 0;
    for(int i=0; i<5; i++)
    {
	for(int j=0; j<8; j++)
        {
            if( (j < L) && (sample < L*5) )
            {
                gfir2[i*8+j] = (coef2[sample]*32767.0);
		sample++;             
            }
            else 
            {
		gfir2[i*8+j] = 0;
            }
	}
    } 
    
    L-=1; 
  
    if (tx)
    {
      if (Modify_SPI_Reg_bits(LMS7param(GFIR1_L_TXTSP),L,true)!=0)
           return -1;   
      Modify_SPI_Reg_bits(LMS7param(GFIR1_N_TXTSP),div,true);
      Modify_SPI_Reg_bits(LMS7param(GFIR2_L_TXTSP),L,true);
      Modify_SPI_Reg_bits(LMS7param(GFIR2_N_TXTSP),div,true);
      Modify_SPI_Reg_bits(LMS7param(GFIR3_L_TXTSP),L,true);
      Modify_SPI_Reg_bits(LMS7param(GFIR3_N_TXTSP),div,true);
          
    }
    else
    {
      if (Modify_SPI_Reg_bits(LMS7param(GFIR1_L_RXTSP),L,true)!=0) 
          return -1;
      Modify_SPI_Reg_bits(LMS7param(GFIR1_N_RXTSP),div,true);
      Modify_SPI_Reg_bits(LMS7param(GFIR2_L_RXTSP),L,true);
      Modify_SPI_Reg_bits(LMS7param(GFIR2_N_RXTSP),div,true);
      Modify_SPI_Reg_bits(LMS7param(GFIR3_L_RXTSP),L,true);
      Modify_SPI_Reg_bits(LMS7param(GFIR3_N_RXTSP),div,true);
              
    }
    
    if ((SetGFIRCoefficients(tx,0,gfir2,40)!=0)
    ||(SetGFIRCoefficients(tx,1,gfir2,40)!=0)
    ||(SetGFIRCoefficients(tx,2,gfir1,120)!=0))
        return -1;
 
  return 0;
}


int LMS7_Device::ConfigureTXLPF(bool enabled,int ch,double bandwidth)
{
    if (ch == 1)
    {
        if (Modify_SPI_Reg_bits(LMS7param(MAC),2,true)!=0)
            return -1;
    }
    else
    {
       if (Modify_SPI_Reg_bits(LMS7param(MAC),1,true)!=0)
           return -1;
    }
    if (enabled)
    {
           
            if (Modify_SPI_Reg_bits(LMS7param(PD_LPFH_TBB),0,true)!=0)
                return -1;
            Modify_SPI_Reg_bits(LMS7param(PD_LPFLAD_TBB),0,true);
            Modify_SPI_Reg_bits(LMS7param(PD_LPFS5_TBB),0,true);
            
            if (bandwidth > 0)
            if (TuneTxFilter(bandwidth)!=0)
                    return -1; 
    }
    else
    {
        Modify_SPI_Reg_bits(LMS7param(PD_LPFLAD_TBB),1,true);
        Modify_SPI_Reg_bits(LMS7param(PD_LPFS5_TBB),1,true);
        return Modify_SPI_Reg_bits(LMS7param(PD_LPFH_TBB),1,true);
    }
    return 0;
}

int LMS7_Device::ConfigureSamplePositions() 
{
    if (Modify_SPI_Reg_bits(LMS7param(LML2_BQP),3,true)!=0)
        return -1;
    Modify_SPI_Reg_bits(LMS7param(LML2_BIP),2,true);
    Modify_SPI_Reg_bits(LMS7param(LML2_AQP),1,true);
    Modify_SPI_Reg_bits(LMS7param(LML2_AIP),0,true); 
    Modify_SPI_Reg_bits(LMS7param(LML1_BQP),3,true);
    Modify_SPI_Reg_bits(LMS7param(LML1_BIP),2,true);
    Modify_SPI_Reg_bits(LMS7param(LML1_AQP),1,true);
    Modify_SPI_Reg_bits(LMS7param(LML1_AIP),0,true);
        
    Modify_SPI_Reg_bits(LMS7param(LML1_S3S),2,true);
    Modify_SPI_Reg_bits(LMS7param(LML1_S2S),3,true);
    Modify_SPI_Reg_bits(LMS7param(LML1_S1S),0,true);
    Modify_SPI_Reg_bits(LMS7param(LML1_S0S),1,true);
    Modify_SPI_Reg_bits(LMS7param(LML2_S3S),2,true);
    Modify_SPI_Reg_bits(LMS7param(LML2_S2S),3,true);
    Modify_SPI_Reg_bits(LMS7param(LML2_S1S),0,true);
    Modify_SPI_Reg_bits(LMS7param(LML2_S0S),1,true);

   return 0;
}

LMS7_Device::~LMS7_Device()
{
    delete [] tx_channels;
    delete [] rx_channels;
}

int LMS7_Device::SetReferenceClock(const double refCLK_Hz)
{
    SetReferenceClk_SX(false, refCLK_Hz);
    SetReferenceClk_SX(true, refCLK_Hz);
    return 0;
}

size_t LMS7_Device::GetNumChannels(const bool tx) const
{
    return 2;
}

int LMS7_Device::SetRate(float_type f_Hz, int oversample)
{
   int decim = 0;
   
   if (oversample > 1)
   {
       for (decim = 0; decim < 4; decim++)
       {
            if ( (1<<decim) >= (oversample+1)/2)
            {
             break;  
            }
       }
   }
   else if (oversample == 0)
       decim = 0;
   else decim = 7;
   
   int ratio = oversample <= 2 ? 2 : (2<<decim);
   float_type cgen = f_Hz*4*ratio;
   if (cgen > LMS_CGEN_MAX)
   {
       lime::ReportError(ERANGE, "Cannot set desired sample rate. CGEN clock out of range");
       return -1;
   }
   if ((SetFrequencyCGEN(cgen)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(EN_ADCCLKH_CLKGN),0)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(CLKH_OV_CLKL_CGEN),2)!=0) 
   ||(Modify_SPI_Reg_bits(LMS7param(MAC),2,true)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP), decim)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(HBI_OVR_TXTSP), decim)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(MAC),1,true)!=0)
   ||(SetInterfaceFrequency(GetFrequencyCGEN(),decim,decim)!=0))
           return -1;

    float_type fpgaTxPLL = GetReferenceClk_TSP(lime::LMS7002M::Tx) /
                            pow(2.0, Get_SPI_Reg_bits(LMS7param(HBI_OVR_TXTSP)));
    float_type fpgaRxPLL = GetReferenceClk_TSP(lime::LMS7002M::Rx) /
                            pow(2.0, Get_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP)));

    this->streamPort->UpdateExternalDataRate(0,fpgaTxPLL/2,fpgaRxPLL/2);
   return 0;
}

int LMS7_Device::SetRate(bool tx, float_type f_Hz, size_t oversample)
{
    float_type tx_clock;
    float_type rx_clock;
    float_type cgen;
    
    int decimation;
    int interpolation;
    int tmp;
    
    if (oversample > 1)
    {
        for (tmp = 0; tmp < 4; tmp++)
        {
             if ( (1<<tmp) >= (oversample+1)/2)
             {
              break;  
             }
        }
    }
    else if (oversample == 0)
        tmp = 0;
    else tmp = 7;
    
    int ratio = oversample <= 2 ? 2 : (2<<tmp); 
    
    if (tx)
    {
        interpolation = tmp;
        decimation = Get_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP));
        rx_clock = GetReferenceClk_TSP(lime::LMS7002M::Rx);
        tx_clock = f_Hz*ratio;
        cgen = tx_clock;
    }
    else
    {
        decimation = tmp;
        interpolation = Get_SPI_Reg_bits(LMS7param(HBI_OVR_TXTSP));
        tx_clock = GetReferenceClk_TSP(lime::LMS7002M::Tx);
        rx_clock = f_Hz * ratio;
        cgen = rx_clock * 4;
    }

    int div_index = floor(log2(tx_clock/rx_clock)+0.5); 
    
    while (div_index < -1)
    {
        if (tx)
        {
           if (decimation > 0)
           {
             decimation--;
             div_index++;
           }
           else
           {
               div_index = -1;
               break;
           }
        }
        else
        {
           if (interpolation < 4)
           {
             interpolation++;
             div_index++;
           }
           else 
           {
               div_index = -1;
               break; 
           }
        }
    }
    
    while (div_index > 5)
    {
        if (tx)
        {
           if (decimation < 4)
           {
             decimation++;
             div_index--;
           }
           else
           {
             div_index = 5;
             break;
           }
        }
        else
        {
           if (interpolation > 0)
           {
             interpolation--;
             div_index--;
           }
           else
           {
               div_index = 5;
               break;
           }
        }
    }
    
    int clk_mux;
    int clk_div;
    
    switch (div_index)
    {
        case -1://2:1
                clk_mux = 0; 
                clk_div = 3;    
                break;
        case 0://1:1 
                clk_mux = 0;   
                clk_div = 2;
                break;
        case 1: //1:2
                clk_mux = 0; 
                clk_div = 1;
                break;  
        case 2://1:4
                clk_mux = 0; 
                clk_div = 0;
                break;
        case 3: //1:8
                clk_mux = 1;
                clk_div = 1;
                break;
        case 4: //1:16
                clk_mux = 1;
                clk_div = 2;
                break;
        case 5: //1:32
                clk_mux = 1;
                clk_div = 3;
                break; 
    }
    
    if ((tx && clk_mux == 0)||(tx == false && clk_mux == 1)) 
    {
        while (cgen*(1<<clk_div)>LMS_CGEN_MAX)
        {
            if (clk_div > 0)
            {
                clk_div--;
            }
            else
            {
               lime::ReportError(ERANGE, "Cannot set desired sample rate. CGEN clock out of range");
               return -1;
            }
        }
        cgen *= (1 << clk_div);
    }
  
    if (cgen > LMS_CGEN_MAX)
    {
        lime::ReportError(ERANGE, "Cannot set desired sample rate. CGEN clock out of range");
        return -1; 
    }
    
   if ((SetFrequencyCGEN(cgen)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(EN_ADCCLKH_CLKGN),clk_mux)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(CLKH_OV_CLKL_CGEN),clk_div)!=0) 
   ||(Modify_SPI_Reg_bits(LMS7param(MAC),2,true)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP), decimation)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(HBI_OVR_TXTSP), interpolation)!=0)
   ||(Modify_SPI_Reg_bits(LMS7param(MAC),1,true)!=0)
   ||(SetInterfaceFrequency(GetFrequencyCGEN(),interpolation,decimation)!=0))
           return -1;
    
   return 0;
}


double LMS7_Device::GetRate(bool tx, size_t chan,float_type *rf_rate_Hz)
{
     
    double interface_Hz;
    int ratio;
    
    if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
    
    if (tx)
    {
        ratio = Get_SPI_Reg_bits(LMS7param(HBI_OVR_TXTSP));
        interface_Hz = GetReferenceClk_TSP(lime::LMS7002M::Tx);
    }
    else
    {
        ratio = Get_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP));
        interface_Hz = GetReferenceClk_TSP(lime::LMS7002M::Rx);       
    }
   
    if (ratio == 7)
       interface_Hz /= 2; 
   
    if (rf_rate_Hz)
        *rf_rate_Hz = interface_Hz;

    if (ratio != 7)
        interface_Hz /= 2*pow(2.0, ratio);

    return interface_Hz; 
}

lms_range_t LMS7_Device::GetRxRateRange(const size_t chan) const
{
  lms_range_t ret; 
  ret.max = 30000000;
  ret.min = 2500000;
  ret.step = 1;
  return ret;
}

lms_range_t LMS7_Device::GetTxRateRange(size_t chan) const
{
  lms_range_t ret; 
  ret.max = 30000000;
  ret.min = 2500000;
  ret.step = 1;
  return ret;
}

std::vector<std::string> LMS7_Device::GetPathNames(bool dir_tx, size_t chan) const
{  
    if (dir_tx)
    {
       return {"NONE", "LNA_H", "LNA_L", "LNA_W"}; 
    }
    else
    {
       return {"NONE", "TX_PATH1", "TX_PATH2"}; 
    }
}

int LMS7_Device::SetPath(bool tx, size_t chan, size_t path)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
    if (tx==false)
    { 
        if ((Modify_SPI_Reg_bits(LMS7param(SEL_PATH_RFE),path,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_L_RFE),path!=2,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_W_RFE),path!=3,true)!=0))
            return -1;
    }
    else
    {
        if ((Modify_SPI_Reg_bits(LMS7param(SEL_BAND1_TRF),path==2,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(SEL_BAND2_TRF),path==1,true)!=0))
            return -1;
    }   
    return 0;
}

size_t LMS7_Device::GetPath(bool tx, size_t chan)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
    if (tx)
    {
        if (Get_SPI_Reg_bits(LMS7param(SEL_BAND1_TRF),true))
            return 2;
        else if (Get_SPI_Reg_bits(LMS7param(SEL_BAND2_TRF),true))
            return 1;
        else
            return 0;  
    }
    
    return Get_SPI_Reg_bits(LMS7param(SEL_PATH_RFE),true);    
}

lms_range_t LMS7_Device::GetRxPathBand(size_t path, size_t chan) const
{
  lms_range_t ret; 
  switch (path)
  {
      case 1:
            ret.max = 3800000000;
            ret.min = 2000000000;
            ret.step = 1; 
            break;
      case 3:
            ret.max = 3800000000;
            ret.min = 1000000000;
            ret.step = 1; 
            break;
      case 2:
            ret.max = 1000000000;
            ret.min = 100000000;
            ret.step = 1; 
            break;
      default: 
            ret.max = 0;
            ret.min = 0;
            ret.step = 0; 
            break;
  }

  return ret;  
}

lms_range_t LMS7_Device::GetTxPathBand(size_t path, size_t chan) const
{
  lms_range_t ret; 
  switch (path)
  {
      case 1:
            ret.max = 3800000000;
            ret.min = 1500000000;
            ret.step = 1; 
            break;
      case 2:
            ret.max = 1500000000;
            ret.min = 100000000;
            ret.step = 1; 
            break;
      default: 
            ret.max = 0;
            ret.min = 0;
            ret.step = 0; 
            break;
  }

  return ret; 
}

int LMS7_Device::SetLPF(bool tx,size_t chan, bool filt, bool en, float_type bandwidth)
{
    
    if (filt)
    {  
       if (tx)
       {
          if(en)
            tx_channels[chan].lpf_bw = bandwidth; 
          
          return ConfigureTXLPF(en,chan,bandwidth)!=0;
       }
       else
       {
          if (en)
            rx_channels[chan].lpf_bw = bandwidth; 
          
          return ConfigureRXLPF(en,chan,bandwidth);
       }
    }
    else
    {
       return ConfigureGFIR(en,tx,bandwidth,chan);
    }
}

float_type LMS7_Device::GetLPFBW(bool tx,size_t chan, bool filt)
{
    
    if (filt)
    {  
       if (tx)
       {
           return tx_channels[chan].lpf_bw;
       }
       else
       {
            return rx_channels[chan].lpf_bw;
       }
    }
    else
    {
       return -1;
    }
}


lms_range_t LMS7_Device::GetLPFRange(bool tx, size_t chan, bool filt)
{
    lms_range_t ret; 
    if (filt)
    {
      ret.max = 160000000; 
    }
    else
    {
      ret.max = GetRate(false,chan)/1.03; 
    }
    ret.min = 1000000;
    ret.step = 1;
    return ret;  
}


int LMS7_Device::SetGFIRCoef(bool tx, size_t chan, lms_gfir_t filt, const float_type* coef,size_t count)
{
    short gfir[120];
    int L;
    int div = 1;
    int ret = 0;
    
    if (count > 120)
    {
        lime::ReportError(ERANGE, "Max number of coefficients for GFIR3 is 120 and for GFIR1(2) - 40");
        return -1;
    }
    else if (count > 40 && filt != LMS_GFIR3)
    {
        lime::ReportError(ERANGE, "Max number of coefficients for GFIR1(2) is 40");
        return -1;
    }
    
    if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
 
    double interface_Hz;
    int ratio;

    if (tx)
    {
      ratio = Get_SPI_Reg_bits(LMS7param(HBI_OVR_TXTSP));
      interface_Hz = GetReferenceClk_TSP(lime::LMS7002M::Tx);
    }
    else
    {
      ratio =Get_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP));
      interface_Hz = GetReferenceClk_TSP(lime::LMS7002M::Rx);
    }

    if (ratio == 7)
        interface_Hz /= 2;
    else
        div = (2<<(ratio));


    L = div > 8 ? 8 : div;
    div -= 1;
    
    if (filt==LMS_GFIR3)
    {
       if (L*15 < count)
       {
           lime::ReportError(ERANGE, "Too many filter coefficients for current oversampling settings");         
           ret = -1;;
           L = 1+(count-1)/15;
           div = L-1;
       }
    }
    else
    {
       if (L*5 < count)
       {
           lime::ReportError(ERANGE, "Too many filter coefficients for current oversampling settings");
           ret = -1;
           L = 1+(count-1)/5;
           div = L-1;
       }
    }
    
    float_type max=0;
    for (int i=0; i< filt==LMS_GFIR3 ? 120 : 40; i++)
        if (fabs(coef[i])>max)
            max=fabs(coef[i]);
    
    if (max < 1.0)
        max = 1.0;
    
    int sample = 0;
    for(int i=0; i< filt==LMS_GFIR3 ? 15 : 5; i++)
    {
	for(int j=0; j<8; j++)
        {
            if( (j < L) && (sample < count) )
            {
                gfir[i*8+j] = (coef[sample]*32767.0/max);
		sample++;              
            }
            else 
            {
		gfir[i*8+j] = 0;
            }
	}
    } 
 
    L-=1; 

    if (tx)
    {
      if (filt ==LMS_GFIR1)
      {
          if ((Modify_SPI_Reg_bits(LMS7param(GFIR1_L_TXTSP),L,true)!=0)
          || (Modify_SPI_Reg_bits(LMS7param(GFIR1_N_TXTSP),div,true)!=0))
              return -1;
      }
      else if (filt ==LMS_GFIR2)
      {
          if ((Modify_SPI_Reg_bits(LMS7param(GFIR2_L_TXTSP),L,true)!=0)
          || (Modify_SPI_Reg_bits(LMS7param(GFIR2_N_TXTSP),div,true)!=0))
              return -1;
      }
      else if (filt ==LMS_GFIR3)
      {
          if ((Modify_SPI_Reg_bits(LMS7param(GFIR3_L_TXTSP),L,true)!=0) 
          || (Modify_SPI_Reg_bits(LMS7param(GFIR3_N_TXTSP),div,true)!=0))
              return -1;
      }
    }
    else
    {
      if (filt ==LMS_GFIR1)
      {
        if ((Modify_SPI_Reg_bits(LMS7param(GFIR1_L_RXTSP),L,true)!=0) 
        || (Modify_SPI_Reg_bits(LMS7param(GFIR1_N_RXTSP),div,true)!=0))
            return -1;
      }
      else if (filt ==LMS_GFIR2)
      {
        if ((Modify_SPI_Reg_bits(LMS7param(GFIR2_L_RXTSP),L,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(GFIR2_N_RXTSP),div,true)!=0))
            return -1;
      }
      else if (filt ==LMS_GFIR3)
      {
        if ((Modify_SPI_Reg_bits(LMS7param(GFIR3_L_RXTSP),L,true)!=0) 
        || (Modify_SPI_Reg_bits(LMS7param(GFIR3_N_RXTSP),div,true)!=0))
            return -1;
      }
    }
    
   if (SetGFIRCoefficients(tx,filt,gfir,filt==LMS_GFIR3 ? 120 : 40)!=0)
       return -1;
   return ret;
}

int LMS7_Device::GetGFIRCoef(bool tx, size_t chan, lms_gfir_t filt, float_type* coef)
{
   if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
       return -1;
   int16_t coef16[120];
   
   if (GetGFIRCoefficients(tx,filt,coef16,filt==LMS_GFIR3 ? 120 : 40)!=0)
       return -1;
   if (coef != NULL)
   {
       for (int i = 0; i < filt==LMS_GFIR3 ? 120 : 40 ; i++)
       {
           coef[i] = coef16[i];
           coef[i] /= (1<<15);
       }   
   }
   return filt==LMS_GFIR3 ? 120 : 40; 
}

int LMS7_Device::SetGFIR(bool tx, size_t chan, lms_gfir_t filt, bool enabled)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
    if (tx)
    {
        if (filt ==LMS_GFIR1)
        {
            if (Modify_SPI_Reg_bits(LMS7param(GFIR1_BYP_TXTSP),enabled==false,true)!=0)
                return -1;
        }
        else if (filt == LMS_GFIR2)
        {
            if (Modify_SPI_Reg_bits(LMS7param(GFIR2_BYP_TXTSP),enabled==false,true)!=0)
                return -1;
        }
        else if (filt == LMS_GFIR3)
        {
            if (Modify_SPI_Reg_bits(LMS7param(GFIR3_BYP_TXTSP),enabled==false,true)!=0)
                return -1;
        }
    }
    else       
    {
        if (filt ==LMS_GFIR1)
        {
            if (Modify_SPI_Reg_bits(LMS7param(GFIR1_BYP_RXTSP),enabled==false,true)!=0)
                return -1;
        }
        else if (filt == LMS_GFIR2)
        {
            if (Modify_SPI_Reg_bits(LMS7param(GFIR2_BYP_RXTSP),enabled==false,true)!=0)
                return -1;
        }
        else if (filt == LMS_GFIR3)
        {
            if (Modify_SPI_Reg_bits(LMS7param(GFIR3_BYP_RXTSP),enabled==false,true)!=0)
                return -1;
        }
    }  
    
   return 0; 
}

int LMS7_Device::SetNormalizedGain(bool dir_tx, size_t chan,double gain)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
    
    if (dir_tx)
    {
        int g = (63*gain+0.49);
        if (Modify_SPI_Reg_bits(LMS7param(CG_IAMP_TBB),g,true)!=0)
            return -1;
    }
    else
    {
        const int w_lna = 2;
        const int w_tia = 5;
        const int w_pga = 1;
        const int w_total = 14*w_lna + 2*w_tia + 31*w_pga;
        int target = gain*w_total+0.49;
        unsigned lna = 0, tia = 0, pga = 11;
        int sum = w_lna*lna+w_tia*tia+w_pga*pga;
        
        while (target > sum)
        {
            if (tia < 2)
            {
                tia++;
                sum += w_tia;
            }
            else if (lna < 14)
            {
                lna++;
                sum += w_lna;
            }
            else
            {
                pga++;
                sum += w_pga;
            }
        }

        if (target < sum)
        {
            pga--;
            sum -= w_pga;
        }
             
        if ((Modify_SPI_Reg_bits(LMS7param(G_LNA_RFE),lna+1,true)!=0)
          ||(Modify_SPI_Reg_bits(LMS7param(G_TIA_RFE),tia+1,true)!=0)
          ||(Modify_SPI_Reg_bits(LMS7param(G_PGA_RBB),pga,true)!=0))
            return -1;
    }
    return 0;
}

double LMS7_Device::GetNormalizedGain(bool dir_tx, size_t chan)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
    if (dir_tx)
    {
        double ret = Get_SPI_Reg_bits(LMS7param(CG_IAMP_TBB),true);
        ret /= 63.0;
        return ret;
    }
    else
    {
        const int w_lna = 2;
        const int w_tia = 5;
        const int w_pga = 1;
        const int w_total = 14*w_lna + 2*w_tia + 31*w_pga;         
        int lna = Get_SPI_Reg_bits(LMS7param(G_LNA_RFE),true)-1;
        int tia = Get_SPI_Reg_bits(LMS7param(G_TIA_RFE),true)-1;
        int pga = Get_SPI_Reg_bits(LMS7param(G_PGA_RBB),true);
        double ret = lna*w_lna+tia*w_tia+pga*w_pga;
        ret /= w_total;
        return ret;
    }
    return 0;
}

int LMS7_Device::SetTestSignal(bool dir_tx, size_t chan,lms_testsig_t sig, int16_t dc_i, int16_t dc_q)
{     
    if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
    
    if (dir_tx==false)
    {      
        if ((Modify_SPI_Reg_bits(LMS7param(INSEL_RXTSP),sig!=LMS_TESTSIG_NONE,true)!=0)
        ||(Modify_SPI_Reg_bits(LMS7param(TSGFCW_RXTSP),sig,true)!=0)
        ||(Modify_SPI_Reg_bits(LMS7param(TSGMODE_RXTSP),sig==LMS_TESTSIG_DC,true)!=0))
            return -1;
    }
    else
    {
        if ((Modify_SPI_Reg_bits(LMS7param(INSEL_TXTSP),sig!=LMS_TESTSIG_NONE,true)!=0)
        ||(Modify_SPI_Reg_bits(LMS7param(TSGFCW_TXTSP),sig,true)!=0)
        ||(Modify_SPI_Reg_bits(LMS7param(TSGMODE_TXTSP),sig==LMS_TESTSIG_DC,true)!=0))
            return -1;
    }
    
   if (sig==LMS_TESTSIG_DC)
        return LoadDC_REG_IQ(dir_tx,dc_i,dc_q);
    
    return 0;  
}

int LMS7_Device::GetTestSignal(bool dir_tx, size_t chan)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),chan+1,true)!=0)
        return -1;
    
    if (dir_tx==false)
    {   
        if (Get_SPI_Reg_bits(LMS7param(INSEL_TXTSP),true)==0)
        {
            return LMS_TESTSIG_NONE;
        }
        else if (Get_SPI_Reg_bits(LMS7param(TSGMODE_TXTSP),true)!=0)
        {
            return LMS_TESTSIG_DC;
        }  
        else return Get_SPI_Reg_bits(LMS7param(TSGFCW_TXTSP),true);
    }
    else
    {
        if (Get_SPI_Reg_bits(LMS7param(INSEL_RXTSP),true)==0)
        {
            return LMS_TESTSIG_NONE;
        }
        else if (Get_SPI_Reg_bits(LMS7param(TSGMODE_RXTSP),true)!=0)
        {
            return LMS_TESTSIG_DC;
        }  
        else return Get_SPI_Reg_bits(LMS7param(TSGFCW_RXTSP),true);
    }
    return 0;  
}


int LMS7_Device::SetNCOFreq(bool tx, size_t ch, const float_type *freq, float_type pho)
{ 
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -1;
    
    float_type rf_rate;
    GetRate(tx,ch,&rf_rate);
    rf_rate /=2;
    for (int i = 0; i < 16; i++)
    {
        if (freq[i] < 0 || freq[i] > rf_rate)
        {
            lime::ReportError(ERANGE, "NCO frequency is negative or outside of RF bandwidth range");
            return -1;
        }
        if (SetNCOFrequency(tx,i,freq[i])!=0)
            return -1;
    }
    if (tx)
    {
        if ((Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_TXTSP),0,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(SEL_TX),0,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(CMIX_SC_TXTSP),0,true)!=0))
            return -1;
        if (Modify_SPI_Reg_bits(LMS7param(MODE_TX),0,true)!=0)
            return -1;
        tx_channels[ch].nco_pho = pho;
    }
    else
    {
        if ((Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP),0,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(SEL_RX),0,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(CMIX_SC_RXTSP),0,true)!=0))
            return -1;
        if (Modify_SPI_Reg_bits(LMS7param(MODE_RX),0,true)!=0)
            return -1;
        rx_channels[ch].nco_pho = pho;
    }
    return SetNCOPhaseOffsetForMode0(tx,pho);
}

int LMS7_Device::SetNCO(bool tx,size_t ch,size_t ind,bool down)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -1;
    if (ind == -1)
    {
        if (tx)
        {
            if (Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_TXTSP),1,true)!=0)
                return -1;
        }
        else
        {
            if (Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP),1,true)!=0)
                return -1;
        }
    }
    else
    {
        if (tx)
        {
            if ((Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_TXTSP),0,true)!=0)
            || (Modify_SPI_Reg_bits(LMS7param(SEL_TX),ind,true)!=0)
            || (Modify_SPI_Reg_bits(LMS7param(CMIX_SC_TXTSP),down,true)!=0))
                return -1;
        }
        else
        {
            if ((Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP),0,true)!=0)
            || (Modify_SPI_Reg_bits(LMS7param(SEL_RX),ind,true)!=0)
            || (Modify_SPI_Reg_bits(LMS7param(CMIX_SC_RXTSP),down,true)!=0))
                return -1;
        }
    }
    return 0;
}


int LMS7_Device::GetNCOFreq(bool tx, size_t ch, float_type *freq,float_type *pho)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -1;
    for (int i = 0; i < 16; i++)
    {
        freq[i] = GetNCOFrequency(tx,i,true);
        uint16_t neg = tx ? Get_SPI_Reg_bits(LMS7param(CMIX_SC_TXTSP),true) : Get_SPI_Reg_bits(LMS7param(CMIX_SC_RXTSP),true);
        freq[i] = neg ? -freq[i] : freq[i];
    }
    *pho = tx ? tx_channels[ch].nco_pho : rx_channels[ch].nco_pho;
    return 0;
}

int LMS7_Device::SetNCOPhase(bool tx, size_t ch, const float_type *phase, float_type fcw)
{ 
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -1;

    for (int i = 0; i < 16; i++)
    {
        if (SetNCOPhaseOffset(tx,i,phase[i])!=0)
            return -1;
    }
    
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -1;
    
    if (tx)
    {
        if ((Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_TXTSP),0,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(SEL_TX),0,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(CMIX_SC_TXTSP),0,true)!=0))
            return -1;
        if (Modify_SPI_Reg_bits(LMS7param(MODE_TX),1,true)!=0)
            return -1;
        tx_channels[ch].nco_pho = fcw;
    }
    else
    {
        if ((Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP),0,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(SEL_RX),0,true)!=0)
        || (Modify_SPI_Reg_bits(LMS7param(CMIX_SC_RXTSP),0,true)!=0))
            return -1;
        if (Modify_SPI_Reg_bits(LMS7param(MODE_RX),1,true)!=0)
            return -1;
        rx_channels[ch].nco_pho = fcw;
    }
    
    return SetNCOFrequency(tx,0,fcw);
}


int LMS7_Device::GetNCOPhase(bool tx, size_t ch, float_type *phase,float_type *fcw)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -1;
    for (int i = 0; i < 16; i++)
    {
        phase[i] = GetNCOPhaseOffset_Deg(tx,i);
    }
    *fcw = tx ? tx_channels[ch].nco_pho : rx_channels[ch].nco_pho;
    return 0;
}

size_t LMS7_Device::GetNCO(bool tx, size_t ch)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -2;
    if (tx)
    {
        if (Get_SPI_Reg_bits(LMS7param(CMIX_BYP_TXTSP),true)!=0)
        {
            lime::ReportError(ENODEV, "NCO is disabled");
            return -1;
        }
        return Get_SPI_Reg_bits(LMS7param(SEL_TX),true);
    }
    
    if (Get_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP),true)!=0)
        return -1;
    return Get_SPI_Reg_bits(LMS7param(SEL_RX),true);
}

int LMS7_Device::SetRxFrequency(size_t chan, double f_Hz)
{
    rx_channels[0].half_duplex = false;
    rx_channels[1].half_duplex = false;
    if (SetFrequencySX(false,f_Hz)!=0)
        return -1;
    if (f_Hz < GetRxPathBand(LMS_PATH_LOW,chan).max)
    {
        SetPath(false,0,LMS_PATH_LOW);
        SetPath(false,1,LMS_PATH_LOW);
        printf("RX LNA_L path selected\n");
    }
    else 
    {
        SetPath(false,0,LMS_PATH_WIDE);
        SetPath(false,1,LMS_PATH_WIDE);
        printf("RX LNA_W path selected\n");
    }
    return 0;  
}

int LMS7_Device::SetTxFrequency(size_t chan, double f_Hz)
{    
    rx_channels[0].half_duplex = false;
    rx_channels[1].half_duplex = false;
    if (SetFrequencySX(true,f_Hz)!=0)
        return -1;
    if (f_Hz < GetTxPathBand(LMS_PATH_LOW,chan).max)
    {
        SetPath(true,0,LMS_PATH_LOW);
        SetPath(true,1,LMS_PATH_LOW);
        printf("TX BAND 1 selected\n");
    }
    else 
    {
        printf("Tx BAND 2 selected\n");
        SetPath(true,0,LMS_PATH_HIGH);
        SetPath(true,1,LMS_PATH_HIGH);
    }
    return 0;  
}


lms_range_t LMS7_Device::GetFrequencyRange(bool tx) const
{
  lms_range_t ret; 
  ret.max = 3800000000;
  ret.min = 100000000;
  ret.step = 1;
  return ret; 
}

int LMS7_Device::Init()
{ 
   if ((ResetChip()!=0)
     ||(Modify_SPI_Reg_bits(LMS7param(MAC),2,true)!=0)
     ||(Modify_SPI_Reg_bits(0x208, 8, 0, 0x1F3, true)!=0)  //TXTSP bypasses
     ||(Modify_SPI_Reg_bits(0x40C, 7, 0, 0xFB, true)!=0)   //RXTSP bypasses
     ||(Modify_SPI_Reg_bits(LMS7param(ICT_LNA_RFE),0x1F,true)!=0)
     ||(Modify_SPI_Reg_bits(LMS7param(ICT_LODC_RFE),0x1F,true)!=0)  
     ||(Modify_SPI_Reg_bits(LMS7param(MAC),1,true)!=0)    
     ||(Modify_SPI_Reg_bits(0x208, 8, 0, 0x1F3, true)!=0)
     ||(Modify_SPI_Reg_bits(0x40C, 7, 0, 0xFB, true)!=0)
     ||(Modify_SPI_Reg_bits(LMS7param(ICT_LNA_RFE),0x1F,true)!=0)
     ||(Modify_SPI_Reg_bits(LMS7param(ICT_LODC_RFE),0x1F,true)!=0)
     ||(Modify_SPI_Reg_bits(LMS7param(LML1_MODE),0,true)!=0)
     ||(Modify_SPI_Reg_bits(LMS7param(LML2_MODE),0,true)!=0)
     ||(Modify_SPI_Reg_bits(LMS7param(DIQ1_DS),1,true)!=0)    
     ||(Modify_SPI_Reg_bits(LMS7param(PD_TX_AFE2),0,true)!=0)
     ||(Modify_SPI_Reg_bits(LMS7param(PD_RX_AFE2),0,true)!=0)) 
        return -1;
   DownloadAll();
   return ConfigureSamplePositions();
}


int LMS7_Device::EnableTX(size_t ch, bool enable)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -1;
    
    this->EnableChannel(true,enable);
    
    return 0;
}

int LMS7_Device::EnableRX(const size_t ch, const bool enable)
{
    if (Modify_SPI_Reg_bits(LMS7param(MAC),ch+1,true)!=0)
        return -1;
   
          
    this->EnableChannel(false,enable);
    Modify_SPI_Reg_bits(LMS7param(PD_LNA_RFE),0,true);
    rx_channels[ch].enabled = enable;
    
    return 0;
}


int LMS7_Device::ProgramFPGA(const char* data, size_t len, lms_target_t mode,lime::IConnection::ProgrammingCallback callback)
{
    if (mode > LMS_TARGET_BOOT)
    {
        lime::ReportError(ENOTSUP, "Unsupported target storage type");
        return -1;
    }
    //device FPGA(2)
    //mode to RAM(0), to FLASH (1)
    return streamPort->ProgramWrite(data,len,mode,2,callback);
}

int LMS7_Device::ProgramFPGA(std::string fname, lms_target_t mode,lime::IConnection::ProgrammingCallback callback)
{
    std::ifstream file(fname);
    int len;
    char* data;
    if (file.is_open())
    {
        file.seekg (0, file.end);
        len = file.tellg();
        file.seekg (0, file.beg);
        data = new char[len];
        file.read(data,len);
        file.close();
    }
    else
    {
        lime::ReportError(ENOENT, "Unable to open the specified file");
        return -1;
    }
    int ret = ProgramFPGA(data,len,mode,callback);
    delete [] data;
    return ret;
}

//TODO: fx3 needs restart
int LMS7_Device::ProgramFW(const char* data, size_t len, lms_target_t mode,lime::IConnection::ProgrammingCallback callback)
{
    if (mode != LMS_TARGET_FLASH && mode != LMS_TARGET_BOOT)
    {
        lime::ReportError(ENOTSUP, "Unsupported target storage type");
        return -1;
    }
    //device fx3(1)
    //mode program_flash(2))
    if (mode == LMS_TARGET_FLASH)
        return streamPort->ProgramWrite(data,len,2,1,callback);
    else
        return streamPort->ProgramWrite(nullptr,0,0,1,callback);
}

int LMS7_Device::ProgramFW(std::string fname, lms_target_t mode,lime::IConnection::ProgrammingCallback callback)
{
    std::ifstream file(fname);
    int len;
    char* data;
    if (file.is_open())
    {
        file.seekg (0, file.end);
        len = file.tellg();
        file.seekg (0, file.beg);
        data = new char[len];
        file.read(data,len);
        file.close();
    }
    else
    {
        lime::ReportError(ENOENT, "Unable to open the specified file");
        return -1;
    }
    int ret = ProgramFW(data,len,mode,callback);
    delete [] data;
    return ret;
}

int LMS7_Device::ProgramMCU(const char* data, size_t len, lms_target_t target,lime::IConnection::ProgrammingCallback callback)
{
    lime::MCU_BD mcu;
    lime::MCU_BD::MEMORY_MODE mode;
    uint8_t bin[8192];
    
    if ((data == nullptr)||(target==LMS_TARGET_BOOT)) //boot from FLAH
    {
        mode = lime::MCU_BD::SRAM_FROM_EEPROM;
    }
    else
    { 
        memcpy(bin,data,len>sizeof(bin) ? sizeof(bin) : len);
    }
    
    if (target == LMS_TARGET_RAM)
    {
        mode = lime::MCU_BD::SRAM;
    }
    else if (target == LMS_TARGET_FLASH)
    {
        mode = lime::MCU_BD::EEPROM_AND_SRAM;
    }
    else
    {
        lime::ReportError(ENOTSUP, "Unsupported target storage type");
        return -1;
    }
    mcu.Initialize(GetConnection());
    mcu.callback = callback;
    mcu.Program_MCU(bin,mode);
    mcu.callback = nullptr;
    return 0;
}

int LMS7_Device::DACWrite(uint16_t val)
{
    uint8_t id=0;
    double dval= val; 
    return streamPort->CustomParameterWrite(&id,&dval,1,NULL);
}

int LMS7_Device::DACRead()
{
    uint8_t id=0;
    double dval=0; 
    int ret = streamPort->CustomParameterRead(&id,&dval,1,NULL);
    return ret >=0 ? dval : -1;
}









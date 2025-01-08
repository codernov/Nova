#include "../helper/measurements.h"
#include <stdint.h>





// Clamp value between min and max
//---------------------------------------------------------------------------
long long Clamp(long long v, long long min, long long max) 
  {
    return v <= min ? min : (v >= max ? max : v);
  }
  
// Clamp float value between min and max
//---------------------------------------------------------------------------
uint32_t ClampF(uint32_t v, uint32_t min, uint32_t max) 
  {
    return v <= min ? min : (v >= max ? max : v);
  }

// Convert domain for a value from min max to new min max
//---------------------------------------------------------------------------
int ConvertDomain(int aValue, int aMin, int aMax, int bMin, int bMax) 
  {
    const int aRange = aMax - aMin;
    const int bRange = bMax - bMin;

    aValue = Clamp(aValue, aMin, aMax);
  
    return ((aValue - aMin) * bRange + aRange / 2) / aRange + bMin;
  }

// Convet domain, float value from min max to new min max
//---------------------------------------------------------------------------
uint32_t ConvertDomainF( uint32_t aValue, uint32_t aMin, uint32_t aMax,
                         uint32_t bMin, uint32_t bMax
                       ) 
  {
    const uint64_t aRange = aMax - aMin;
    const uint64_t bRange = bMax - bMin;

    aValue = ClampF(aValue, aMin, aMax);
    
    return ((aValue - aMin) * bRange + aRange / 2) / aRange + bMin;
  }


// Convert dBm to S for HF and VHF
//---------------------------------------------------------------------------
uint8_t DBm2S(int dbm, bool isVHF) {
  uint8_t i = 0;
  
  dbm *= -1;
  
  for (i = 0; i < 15; i++) 
    {
      if (dbm >= rssi2s[isVHF][i]) 
        return i;     
    }
  
  return i;
}

// Convert RSSI to dBm
//---------------------------------------------------------------------------
int Rssi2DBm(uint16_t rssi) 
  { 
    return (rssi >> 1) - 160; 
  }

// applied x2 to prevent initial rounding
//---------------------------------------------------------------------------
uint8_t Rssi2PX(uint16_t rssi, uint8_t pxMin, uint8_t pxMax) 
  {
    return ConvertDomain(rssi - 320, -260, -120, pxMin, pxMax);
  }

// Mean average value of array
//---------------------------------------------------------------------------
uint16_t Mean(const uint16_t *array, uint8_t n) 
  {
    int32_t sum = 0;
    
    for (uint8_t i = 0; i < n; ++i) 
      {
        sum += array[i];
      }
       
    return sum / n;
  }
  
// Minimum value of array
//---------------------------------------------------------------------------
uint16_t Min(const uint16_t *array, uint8_t n) 
  {
    uint16_t min = array[0];
  
    for (uint8_t i = 1; i < n; ++i) 
      {
        if (array[i] < min) 
          {
            min = array[i];
          }
      }
      
    return min;
  }

// Maximum value of an array
//---------------------------------------------------------------------------
uint16_t Max(const uint16_t *array, uint8_t n) 
  {
    uint16_t max = array[0];
  
    for (uint8_t i = 1; i < n; ++i) 
      {
        if (array[i] > max) 
          {
            max = array[i];
          }
      }
      
    return max;
  }
  
// Returns minimum of two
// Made it a function for size reduction purposes
//---------------------------------------------------------------------------
uint16_t MIN(uint16_t a, uint16_t b)
  {
    uint16_t _a = (a);                                                    
    uint16_t _b = (b);   
                                                     
    return _a < _b ? _a : _b;                                                        
  }  

// Return square root
//---------------------------------------------------------------------------
uint32_t SQRT16(uint32_t v) 
  {
    unsigned int shift = 16; // number of bits supplied in 'value' .. 2 ~ 32
    unsigned int bit = 1u << --shift;
    unsigned int sqrti = 0;
  
    while (bit) 
      {
        const unsigned int temp = ((sqrti << 1) | bit) << shift--;
        if (v >= temp) 
          {
            v -= temp;
            sqrti |= bit;
          }
    
        bit >>= 1;
      }

    return sqrti; 
  }

// Wrongly calculate standard deviation, this returns square root of the average
// squares of data TODO: see if proper implementation makes a difference
//---------------------------------------------------------------------------
uint16_t Std(const uint16_t *data, uint8_t n) 
  {
    uint32_t sumDev = 0;

    for (uint8_t i = 0; i < n; ++i) 
      {
        sumDev += data[i] * data[i];
      }
      
    return SQRT16(sumDev / n);
  }

// Increment or decrement unsigned val within range
//---------------------------------------------------------------------------
void IncDec8(uint8_t *val, uint8_t min, uint8_t max, int8_t inc) 
  {
    if (inc > 0) 
      {
        *val = *val == max - inc ? min : *val + inc;
      } 
    else 
      {
        *val = *val > min ? *val + inc : max + inc;
      }
  }

// Increment or decrement signed val within range
//---------------------------------------------------------------------------
void IncDecI8(int8_t *val, int8_t min, int8_t max, int8_t inc) 
  {
    if (inc > 0) 
      {
        *val = *val == max - inc ? min : *val + inc;
      } 
    else 
      {
        *val = *val > min ? *val + inc : max + inc;
      }
  }

// Increment or decrement unsigned, 16 bit version
//---------------------------------------------------------------------------
void IncDec16(uint16_t *val, uint16_t min, uint16_t max, int16_t inc) 
  {
    if (inc > 0) 
      {
        *val = *val == max - inc ? min : *val + inc;
      } 
    else 
      {
        *val = *val > min ? *val + inc : max + inc;
      }
  }

// Increment or decrement signed, 16 bit version
//---------------------------------------------------------------------------
void IncDecI16(int16_t *val, int16_t min, int16_t max, int16_t inc) 
  {
    if (inc > 0) 
      {
        *val = *val == max - inc ? min : *val + inc;
      } 
    else 
      {
        *val = *val > min ? *val + inc : max + inc;
      }
  }

// Increment or decrement signed, 32 bit version
//---------------------------------------------------------------------------
void IncDecI32(int32_t *val, int32_t min, int32_t max, int32_t inc) 
  {
    if (inc > 0) 
      {
        *val = *val == max - inc ? min : *val + inc;
      } 
    else 
      {
        *val = *val > min ? *val + inc : max + inc;
      }
  }

// Increment or decrement unsigned, 32 bit version
//---------------------------------------------------------------------------
void IncDec32(uint32_t *val, uint32_t min, uint32_t max, int32_t inc) 
  {
    if (inc > 0) 
      {
        *val = *val == max - inc ? min : *val + inc;
      } 
    else 
      {
        *val = *val > min ? *val + inc : max + inc;
      }
  }

// Is name ASCII characters
//---------------------------------------------------------------------------
bool IsReadable(char *name) 
  { 
    return name[0] >= 32 && name[0] < 127; 
  }

// Returns SQL elements
//---------------------------------------------------------------------------
SQL GetSql(uint8_t level) 
  {
    SQL sq = {0, 0, 255, 255, 255, 255};
  
    if (level == 0) 
      {
        return sq;
      }

    sq.ro = ConvertDomain(level, 0, 15, 10, 180);
    sq.no = ConvertDomain(level, 0, 15, 64, 12);
    sq.go = ConvertDomain(level, 0, 15, 32, 6);

    sq.rc = sq.ro - 6;
    sq.nc = sq.gc = sq.no + 6;
  
    return sq;
  }

//---------------------------------------------------------------------------














#include <stdint.h>
#include "misc.h"



// Is char printable, is it in fonts
//---------------------------------------------------------------------------
char IsPrintable(char ch) 
  { 
    return (ch < 32 || 126 < ch) ? ' ' : ch; 
  }
  
//---------------------------------------------------------------------------



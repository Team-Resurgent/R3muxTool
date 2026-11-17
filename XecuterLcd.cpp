//-----------------------------------------------------------------------------
// XecuterLcd.cpp
//
// Ported from PrometheOS: PrometheOSXbe/DisplayDrivers/displayHD44780Xecuter.cpp
//-----------------------------------------------------------------------------
#include "XecuterLcd.h"
#include <conio.h>
#include <string.h>

namespace
{
    const DWORD LCD_DELAY = 2;  // ms

    // Control lines on REG_DISP_O_CMD
    const BYTE CON_RS = 0x01;
    const BYTE CON_E  = 0x04;

    // writeValue() mode flags
    const BYTE MODE_INI = 0x01;  // high nibble only (8-bit init sequence)
    const BYTE MODE_CMD = 0x00;
    const BYTE MODE_DAT = 0x02;

    // HD44780 instructions
    const BYTE DISP_CLEAR           = 0x01;
    const BYTE DISP_ENTRY_MODE_SET  = 0x04;
    const BYTE DISP_ID_FLAG         = 0x02;
    const BYTE DISP_CONTROL         = 0x08;
    const BYTE DISP_D_FLAG          = 0x04;
    const BYTE DISP_FUNCTION_SET    = 0x20;
    const BYTE DISP_DL_FLAG         = 0x10;
    const BYTE DISP_N_FLAG          = 0x08;
    const BYTE DISP_DDRAM_SET       = 0x80;

    inline void OutByte( USHORT port, BYTE value ) { _outp( port, value ); }
    inline BYTE InByte( USHORT port )              { return (BYTE)_inp( port ); }

    inline BYTE Scale( BYTE percent )
    {
        if( percent > 100 ) percent = 100;
        return (BYTE)( percent * 1.27f );
    }

    void WriteValue( BYTE value, BYTE mode )
    {
        BYTE cmd = 0;
        if( mode & MODE_DAT )
            cmd |= CON_RS;

        OutByte( XecuterLcd::REG_DISP_O_DAT, value & 0xF0 );
        OutByte( XecuterLcd::REG_DISP_O_CMD, cmd );
        OutByte( XecuterLcd::REG_DISP_O_CMD, CON_E | cmd );
        OutByte( XecuterLcd::REG_DISP_O_CMD, cmd );
        Sleep( LCD_DELAY );

        if( ( mode & MODE_INI ) == 0 )
        {
            OutByte( XecuterLcd::REG_DISP_O_DAT, ( value << 4 ) & 0xF0 );
            OutByte( XecuterLcd::REG_DISP_O_CMD, cmd );
            OutByte( XecuterLcd::REG_DISP_O_CMD, CON_E | cmd );
            OutByte( XecuterLcd::REG_DISP_O_CMD, cmd );
            Sleep( LCD_DELAY );
        }
    }
}

void XecuterLcd::Init( BYTE backlight )
{
    OutByte( REG_DISP_O_DAT, 0 );
    OutByte( REG_DISP_O_CMD, 0 );
    OutByte( REG_DISP_O_DIR_DAT, 0xFF );
    OutByte( REG_DISP_O_DIR_CMD, 0x07 );

    WriteValue( DISP_FUNCTION_SET | DISP_DL_FLAG, MODE_INI ); Sleep( LCD_DELAY );
    WriteValue( DISP_FUNCTION_SET | DISP_DL_FLAG, MODE_INI ); Sleep( LCD_DELAY );
    WriteValue( DISP_FUNCTION_SET | DISP_DL_FLAG, MODE_INI ); Sleep( LCD_DELAY );
    WriteValue( DISP_FUNCTION_SET, MODE_INI );                Sleep( LCD_DELAY );
    WriteValue( DISP_FUNCTION_SET | DISP_N_FLAG, MODE_CMD );  Sleep( LCD_DELAY );
    WriteValue( DISP_CONTROL | DISP_D_FLAG, MODE_CMD );       Sleep( LCD_DELAY );
    WriteValue( DISP_CLEAR, MODE_CMD );                       Sleep( LCD_DELAY );
    WriteValue( DISP_ENTRY_MODE_SET | DISP_ID_FLAG, MODE_CMD ); Sleep( LCD_DELAY );

    SetBacklight( backlight );
}

bool XecuterLcd::SupportsContrast()
{
    return InByte( REG_VERSION_2 ) == 0x69;
}

void XecuterLcd::SetBacklight( BYTE backlight )
{
    OutByte( REG_DISP_O_LIGHT, Scale( backlight ) );
    Sleep( LCD_DELAY );
}

void XecuterLcd::SetContrast( BYTE contrast )
{
    OutByte( REG_DISP_O_CONTRAST, Scale( contrast ) );
    Sleep( LCD_DELAY );
}

void XecuterLcd::Clear()
{
    WriteValue( DISP_CLEAR, MODE_CMD );
    Sleep( LCD_DELAY );
}

void XecuterLcd::SetCursorPosition( BYTE row, BYTE col )
{
    static const BYTE rowOffsets[ROWS] = { 0x00, 0x40, 0x14, 0x54 };
    if( row >= ROWS ) row = ROWS - 1;
    WriteValue( DISP_DDRAM_SET | ( col + rowOffsets[row] ), MODE_CMD );
    Sleep( LCD_DELAY );
}

void XecuterLcd::PrintMessage( const char* message )
{
    for( const char* p = message; *p; ++p )
        WriteValue( (BYTE)*p, MODE_DAT );
}

void XecuterLcd::PrintLines( const char* const* lines, int count )
{
    Clear();
    for( int row = 0; row < count && row < ROWS; ++row )
    {
        char buf[COLS + 1];
        int len = lines[row] ? (int)strlen( lines[row] ) : 0;
        if( len > COLS ) len = COLS;
        memset( buf, ' ', COLS );
        if( len ) memcpy( buf, lines[row], len );
        buf[COLS] = 0;

        SetCursorPosition( (BYTE)row, 0 );
        PrintMessage( buf );
    }
}

//-----------------------------------------------------------------------------
// Bank switches
//-----------------------------------------------------------------------------
bool Xecuter::IsPresent()
{
    return InByte( REG_VERSION ) == 0xE1;
}

bool Xecuter::ReadBankSwitches( BYTE* value )
{
    if( InByte( REG_STATUS ) & STATUS_SOFTCONTROL )
        return false;
    *value = InByte( REG_CONTROL ) & 0x0F;
    return true;
}

const char* Xecuter::BankName( BYTE value )
{
    static const char* const names[16] =
    {
        "Bank 1", "Bank 2", "Bank 3", "Bank 4", "Bank 5", "Bank 6", "Bank 7", "Bank 8",
        "Bank 1-2", "Bank 3-4", "Bank 5-6", "Bank 7-8",
        "Bank 1-4", "Bank 5-8",
        "Bank 1-8", "Bank 1-8",
    };
    return names[value & 0x0F];
}

const char* Xecuter::BankSize( BYTE value )
{
    value &= 0x0F;
    if( value < 8 )  return "256K";
    if( value < 12 ) return "512K";
    if( value < 14 ) return "1M";
    return "2M";
}

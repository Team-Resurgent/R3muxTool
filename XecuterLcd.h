//-----------------------------------------------------------------------------
// XecuterLcd.h
//
// Driver for an HD44780 character LCD on a Team Xecuter modchip LCD port
// (X3 / X3CE style), driven through the chip's LPC I/O registers.
//
// Ported from PrometheOS: PrometheOSXbe/DisplayDrivers/displayHD44780Xecuter.cpp
//-----------------------------------------------------------------------------
#pragma once

#include <xtl.h>

namespace XecuterLcd
{
    // LPC I/O registers exposed by the Xecuter chip
    const USHORT REG_DISP_O_LIGHT    = 0xF503;
    const USHORT REG_DISP_O_DAT      = 0xF504;
    const USHORT REG_DISP_O_CMD      = 0xF505;
    const USHORT REG_DISP_O_DIR_DAT  = 0xF506;
    const USHORT REG_DISP_O_DIR_CMD  = 0xF507;
    const USHORT REG_DISP_O_CONTRAST = 0xF508;
    const USHORT REG_VERSION_2       = 0xF509;

    const int ROWS = 4;
    const int COLS = 20;

    // Bring the LCD up in 4-bit mode, 2+ lines, display on, cleared.
    // backlight is 0-100.
    void Init( BYTE backlight );

    // Contrast control only exists on chips whose VERSION_2 register reads 0x69.
    bool SupportsContrast();

    // 0-100, scaled to the chip's 0-127 range.
    void SetBacklight( BYTE backlight );
    void SetContrast( BYTE contrast );

    void Clear();
    void SetCursorPosition( BYTE row, BYTE col );
    void PrintMessage( const char* message );

    // Clear, then write each line (padded/truncated to COLS) to rows 0..count-1.
    void PrintLines( const char* const* lines, int count );
}

namespace Xecuter
{
    const USHORT REG_VERSION = 0xF500;
    const USHORT REG_CONTROL = 0xF501;
    const USHORT REG_STATUS  = 0xF502;
    const BYTE   STATUS_SOFTCONTROL = 0x80;

    // True if an X3 answers at the register window.
    bool IsPresent();

    // Physical bank switches as a 4-bit value (switch 1 = bit 0, ON reads 0).
    // Returns false when software banking is active: the CONTROL register
    // then reads back the soft bank state rather than the switches.
    bool ReadBankSwitches( BYTE* value );

    // Name of the bank a switch value selects, e.g. "Bank 1-4", and its size.
    const char* BankName( BYTE value );
    const char* BankSize( BYTE value );
}

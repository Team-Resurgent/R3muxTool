//-----------------------------------------------------------------------------
// R3muxTool.cpp
//
// Simple UI to set the backlight / contrast of an HD44780 LCD attached to a
// Team Xecuter modchip, and send a message to it.
//
// Controls:
//   D-pad Up/Down     select item
//   D-pad Left/Right  adjust value / pick message (sent to the LCD at once)
//   A                 re-initialise the LCD (on that item)
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include "xbapp.h"
#include "xbfont.h"
#include "xbinput.h"
#include "XecuterLcd.h"
#include "LcdFont.h"

namespace
{
    enum MenuItem
    {
        ITEM_BACKLIGHT,
        ITEM_CONTRAST,
        ITEM_MESSAGE,
        ITEM_REINIT,
        ITEM_COUNT
    };

    const WCHAR* const g_ItemNames[ITEM_COUNT] =
    {
        L"Backlight",
        L"Contrast",
        L"Message",
        L"Re-initialise LCD",
    };

    struct MessagePreset
    {
        const WCHAR* name;
        const char*  lines[XecuterLcd::ROWS];  // NULL = filled in at runtime
    };

    const MessagePreset g_Presets[] =
    {
        { L"Welcome",       { "     R3muxTool      ", "  Xecuter LCD test  ", "", "  Hello from RXDK!  " } },
        { L"Hello World",   { "Hello, World!", "", "", "" } },
        { L"Settings",      { NULL, NULL, NULL, NULL } },
        { L"Character test",{ "0123456789ABCDEFGHIJ", "KLMNOPQRSTUVWXYZ!?#*", "abcdefghijklmnopqrst", "uvwxyz()[]<>+-=/:;." } },
    };
    const int PRESET_COUNT = sizeof( g_Presets ) / sizeof( g_Presets[0] );
    const int PRESET_SETTINGS = 2;

    const int STEP = 5;
}

class CR3muxTool : public CXBApplication
{
public:
    CR3muxTool();

    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

private:
    void BuildLines( int preset, char out[XecuterLcd::ROWS][XecuterLcd::COLS + 1] );
    void SendMessage();
    void SetStatus( const WCHAR* status );
    void FillRect( float x, float y, float w, float h, D3DCOLOR color );
    void DrawBar( float x, float y, int value, bool selected, bool enabled );
    void DrawBankSwitches();
    void DrawSwitch( float x, float y, bool known, bool on );

    CXBFont m_Font;

    int   m_Selected;
    int   m_Backlight;
    int   m_Contrast;
    int   m_Preset;

    char  m_LcdShadow[XecuterLcd::ROWS][XecuterLcd::COLS + 1];  // what we last sent
    bool  m_X3Present;
    bool  m_SwitchesValid;     // false while software banking is active
    BYTE  m_Switches;
    float m_SwitchPollTime;
    bool  m_ProtectValid;      // false when TSOP / backup flash is mapped
    bool  m_ProtectOn;
    float m_ProtectPollTime;

    WCHAR m_Status[96];
    float m_StatusTime;
};

void __cdecl main()
{
    CR3muxTool app;
    if( FAILED( app.Create() ) )
        return;
    app.Run();
}

CR3muxTool::CR3muxTool()
    : m_Selected( ITEM_BACKLIGHT ),
      m_Backlight( 80 ),
      m_Contrast( 50 ),
      m_Preset( 0 ),
      m_StatusTime( 0.0f ),
      m_X3Present( false ),
      m_SwitchesValid( false ),
      m_Switches( 0 ),
      m_SwitchPollTime( -1.0f ),
      m_ProtectValid( false ),
      m_ProtectOn( false ),
      m_ProtectPollTime( -1.0f )
{
    memset( m_LcdShadow, 0, sizeof( m_LcdShadow ) );
    m_Status[0] = 0;
}

HRESULT CR3muxTool::Initialize()
{
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    XecuterLcd::Init( (BYTE)m_Backlight );
    XecuterLcd::SetContrast( (BYTE)m_Contrast );
    SendMessage();

    OutputDebugStringA( "R3muxTool: LCD initialised\n" );
    SetStatus( L"LCD initialised" );
    return S_OK;
}

void CR3muxTool::BuildLines( int preset, char out[XecuterLcd::ROWS][XecuterLcd::COLS + 1] )
{
    for( int r = 0; r < XecuterLcd::ROWS; ++r )
    {
        const char* src = g_Presets[preset].lines[r];
        char tmp[64];
        if( preset == PRESET_SETTINGS )
        {
            switch( r )
            {
            case 0:  _snprintf( tmp, sizeof( tmp ), "R3muxTool settings" ); break;
            case 1:  _snprintf( tmp, sizeof( tmp ), "Backlight: %3d%%", m_Backlight ); break;
            case 2:  _snprintf( tmp, sizeof( tmp ), "Contrast:  %3d%%", m_Contrast ); break;
            default: tmp[0] = 0; break;
            }
            tmp[sizeof( tmp ) - 1] = 0;
            src = tmp;
        }

        int len = src ? (int)strlen( src ) : 0;
        if( len > XecuterLcd::COLS ) len = XecuterLcd::COLS;
        memset( out[r], ' ', XecuterLcd::COLS );
        if( len ) memcpy( out[r], src, len );
        out[r][XecuterLcd::COLS] = 0;
    }
}

void CR3muxTool::SendMessage()
{
    BuildLines( m_Preset, m_LcdShadow );
    const char* lines[XecuterLcd::ROWS];
    for( int r = 0; r < XecuterLcd::ROWS; ++r )
        lines[r] = m_LcdShadow[r];
    XecuterLcd::PrintLines( lines, XecuterLcd::ROWS );

    OutputDebugStringA( "R3muxTool: message sent\n" );
    for( int r = 0; r < XecuterLcd::ROWS; ++r )
    {
        char dbg[48];
        _snprintf( dbg, sizeof( dbg ), "  |%s|\n", m_LcdShadow[r] );
        dbg[sizeof( dbg ) - 1] = 0;
        OutputDebugStringA( dbg );
    }
}

void CR3muxTool::SetStatus( const WCHAR* status )
{
    wcsncpy( m_Status, status, 95 );
    m_Status[95] = 0;
    m_StatusTime = m_fAppTime;
}

HRESULT CR3muxTool::FrameMove()
{
    // The switches can be flipped while running, so poll them a few times a second.
    if( m_fAppTime - m_SwitchPollTime > 0.25f || m_SwitchPollTime < 0.0f )
    {
        m_SwitchPollTime = m_fAppTime;
        m_X3Present = Xecuter::IsPresent();
        m_SwitchesValid = m_X3Present && Xecuter::ReadBankSwitches( &m_Switches );
    }

    // The protect check sends the flash a command sequence, so do it less often.
    if( m_fAppTime - m_ProtectPollTime > 1.0f || m_ProtectPollTime < 0.0f )
    {
        m_ProtectPollTime = m_fAppTime;
        m_ProtectValid = m_X3Present && Xecuter::ReadWriteProtect( &m_ProtectOn );
    }

    XBGAMEPAD_EVENT ev = m_DefaultGamepad.Event;

    switch( ev )
    {
    case XBGAMEPAD_DPAD_UP:
        m_Selected = ( m_Selected + ITEM_COUNT - 1 ) % ITEM_COUNT;
        break;

    case XBGAMEPAD_DPAD_DOWN:
        m_Selected = ( m_Selected + 1 ) % ITEM_COUNT;
        break;

    case XBGAMEPAD_DPAD_LEFT:
    case XBGAMEPAD_DPAD_RIGHT:
    {
        int dir = ( ev == XBGAMEPAD_DPAD_RIGHT ) ? 1 : -1;
        WCHAR buf[64];
        if( m_Selected == ITEM_BACKLIGHT )
        {
            int v = m_Backlight + dir * STEP;
            m_Backlight = v < 0 ? 0 : ( v > 100 ? 100 : v );
            XecuterLcd::SetBacklight( (BYTE)m_Backlight );
            if( m_Preset == PRESET_SETTINGS ) SendMessage();
            swprintf( buf, L"Backlight set to %d%%", m_Backlight );
            SetStatus( buf );
        }
        else if( m_Selected == ITEM_CONTRAST )
        {
            int v = m_Contrast + dir * STEP;
            m_Contrast = v < 0 ? 0 : ( v > 100 ? 100 : v );
            XecuterLcd::SetContrast( (BYTE)m_Contrast );
            if( m_Preset == PRESET_SETTINGS ) SendMessage();
            swprintf( buf, L"Contrast set to %d%%", m_Contrast );
            SetStatus( buf );
        }
        else if( m_Selected == ITEM_MESSAGE )
        {
            m_Preset = ( m_Preset + PRESET_COUNT + dir ) % PRESET_COUNT;
            SendMessage();
        }
        break;
    }

    case XBGAMEPAD_A:
        if( m_Selected == ITEM_REINIT )
        {
            XecuterLcd::Init( (BYTE)m_Backlight );
            XecuterLcd::SetContrast( (BYTE)m_Contrast );
            SendMessage();
            SetStatus( L"LCD re-initialised" );
        }
        break;

    default:
        break;
    }

    return S_OK;
}

void CR3muxTool::FillRect( float x, float y, float w, float h, D3DCOLOR color )
{
    D3DRECT rc = { (LONG)x, (LONG)y, (LONG)( x + w ), (LONG)( y + h ) };
    m_pd3dDevice->Clear( 1, &rc, D3DCLEAR_TARGET, color, 1.0f, 0 );
}

void CR3muxTool::DrawBar( float x, float y, int value, bool selected, bool enabled )
{
    const float w = 240.0f, h = 14.0f;
    FillRect( x - 2, y - 2, w + 4, h + 4, selected ? 0xffffcc00 : 0xff505868 );
    FillRect( x, y, w, h, 0xff181c24 );
    D3DCOLOR fill = enabled ? 0xff3aa0ff : 0xff607080;
    if( value > 0 )
        FillRect( x, y, w * value / 100.0f, h, fill );
}

// One DIP slot; knob up = ON.
void CR3muxTool::DrawSwitch( float x, float y, bool known, bool on )
{
    const float slotW = 16, slotH = 32;
    FillRect( x, y, slotW, slotH, 0xff1a1a1a );
    if( !known )
        return;
    float knobH = slotH * 0.5f - 2;
    FillRect( x + 2, on ? y + 2 : y + slotH - 2 - knobH, slotW - 4, knobH, 0xfff2f2f2 );
}

// A 4-way DIP switch like the X3's bank selector, with the bank it selects,
// and the write-protect switch beside it in blue.
// Bank switch 1 is bit 0 of the value and reads 0 when ON (knob up).
void CR3muxTool::DrawBankSwitches()
{
    const float bodyX = 432, bodyY = 26, bodyW = 106, bodyH = 46;
    const float slotW = 16, slotH = 32, gap = 9;
    const float slotX0 = bodyX + ( bodyW - ( 4 * slotW + 3 * gap ) ) * 0.5f;
    const float slotY = bodyY + ( bodyH - slotH ) * 0.5f;

    bool live = m_SwitchesValid;
    FillRect( bodyX, bodyY, bodyW, bodyH, live ? 0xffc0262c : 0xff4a3436 );   // red DIP body
    for( int i = 0; i < 4; ++i )
        DrawSwitch( slotX0 + i * ( slotW + gap ), slotY, live, ( m_Switches & ( 1 << i ) ) == 0 );

    // Protect switch
    const float protX = bodyX + bodyW + 8, protW = 576 - protX;
    FillRect( protX, bodyY, protW, bodyH, m_ProtectValid ? 0xff2a6fdb : 0xff34404a );   // blue body
    DrawSwitch( protX + ( protW - slotW ) * 0.5f, slotY, m_ProtectValid, m_ProtectOn );

    WCHAR buf[64];
    if( !m_X3Present )
        swprintf( buf, L"No X3 detected" );
    else if( !live )
        swprintf( buf, L"Software banking" );
    else
    {
        char tmp[64];
        _snprintf( tmp, sizeof( tmp ), "%s  (%s)", Xecuter::BankName( m_Switches ), Xecuter::BankSize( m_Switches ) );
        tmp[sizeof( tmp ) - 1] = 0;
        int n = 0;
        for( ; tmp[n]; ++n ) buf[n] = (WCHAR)(unsigned char)tmp[n];
        buf[n] = 0;
    }

    const WCHAR* protText = !m_ProtectValid ? L"Protect ?" : ( m_ProtectOn ? L"Protect ON" : L"Protect off" );
    D3DCOLOR protColor = !m_ProtectValid ? 0xff8fa8c0 : ( m_ProtectOn ? 0xffff7a5a : 0xff7fe07f );

    m_Font.Begin();
    m_Font.DrawText( bodyX - 14, 24, live ? 0xffffcc00 : 0xff8fa8c0, buf, XBFONT_RIGHT );
    m_Font.DrawText( bodyX - 14, 52, protColor, protText, XBFONT_RIGHT );
    m_Font.End();
}

HRESULT CR3muxTool::Render()
{
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         0xff10141c, 1.0f, 0L );

    // Header band
    FillRect( 0, 0, 640, 92, 0xff1c2836 );
    FillRect( 0, 92, 640, 2, 0xff3aa0ff );

    DrawBankSwitches();

    m_Font.Begin();
    m_Font.DrawText( 64, 24, 0xffffffff, L"R3muxTool" );
    m_Font.DrawText( 64, 52, 0xff8fa8c0, L"Xecuter LCD" );

    // Menu
    const float menuX = 72, valueX = 272, firstY = 108, rowH = 30;
    float textW, textH;
    m_Font.GetTextExtent( L"Ag", &textW, &textH, TRUE );
    const float hlH = rowH - 2;
    const float hlOff = ( textH - hlH ) * 0.5f;   // centre highlight on the text cell
    for( int i = 0; i < ITEM_COUNT; ++i )
    {
        float y = firstY + i * rowH;
        bool sel = ( i == m_Selected );
        if( sel )
        {
            m_Font.End();
            FillRect( menuX - 16, y + hlOff, 528, hlH, 0xff243447 );
            FillRect( menuX - 16, y + hlOff, 4, hlH, 0xffffcc00 );
            m_Font.Begin();
        }

        D3DCOLOR txt = sel ? 0xffffffff : 0xffb0bccc;
        m_Font.DrawText( menuX, y, txt, g_ItemNames[i] );

        WCHAR buf[64];
        switch( i )
        {
        case ITEM_BACKLIGHT:
        case ITEM_CONTRAST:
        {
            int v = ( i == ITEM_BACKLIGHT ) ? m_Backlight : m_Contrast;
            bool enabled = true;
            m_Font.End();
            DrawBar( valueX, y + ( textH - 14.0f ) * 0.5f, v, sel, enabled );
            m_Font.Begin();
            swprintf( buf, L"%3d%%", v );
            m_Font.DrawText( 576, y, txt, buf, XBFONT_RIGHT );
            break;
        }
        case ITEM_MESSAGE:
            swprintf( buf, L"<  %s  >", g_Presets[m_Preset].name );
            m_Font.DrawText( valueX, y, sel ? 0xffffcc00 : txt, buf );
            break;
        default:
            break;
        }
    }

    m_Font.End();

    // LCD preview: what is currently on the LCD, drawn as a 5x8 dot matrix per
    // character. The backlight value lights the panel, the contrast value sets
    // how dark the lit dots are (and how visible the unlit ones are).
    const int DOT = 3, PITCH = 4;                       // dot size / spacing
    const int CELL_W = 5 * PITCH + 4, CELL_H = 8 * PITCH + 4;
    const int PAD_X = 12, PAD_Y = 10;
    const int panelW = CELL_W * XecuterLcd::COLS - 4 + PAD_X * 2;
    const int panelH = CELL_H * XecuterLcd::ROWS - 4 + PAD_Y * 2;
    const int panelX = ( 640 - panelW ) / 2, panelY = 244;

    FillRect( (float)panelX - 8, (float)panelY - 8, (float)panelW + 16, (float)panelH + 16, 0xff2a2e34 );  // bezel

    float bl = m_Backlight / 100.0f;
    float pr = 0x1c + 0x64 * bl, pg = 0x2a + 0xb6 * bl, pb = 0x06 + 0x1a * bl;   // panel colour
    float ct = m_Contrast / 100.0f;
    float ink = 0.30f + 0.65f * ct;     // how far lit dots move toward dark
    float ghost = 0.04f + 0.10f * ct;   // how visible unlit dots are
    const float ir = 0x0a, ig = 0x1e, ib = 0x08;  // ink colour
    D3DCOLOR panelColor = D3DCOLOR_XRGB( (int)pr, (int)pg, (int)pb );
    D3DCOLOR onColor    = D3DCOLOR_XRGB( (int)( pr + ( ir - pr ) * ink ),   (int)( pg + ( ig - pg ) * ink ),   (int)( pb + ( ib - pb ) * ink ) );
    D3DCOLOR offColor   = D3DCOLOR_XRGB( (int)( pr + ( ir - pr ) * ghost ), (int)( pg + ( ig - pg ) * ghost ), (int)( pb + ( ib - pb ) * ghost ) );

    FillRect( (float)panelX, (float)panelY, (float)panelW, (float)panelH, panelColor );

    static D3DRECT onRects[XecuterLcd::ROWS * XecuterLcd::COLS * 40];
    static D3DRECT offRects[XecuterLcd::ROWS * XecuterLcd::COLS * 40];
    DWORD onCount = 0, offCount = 0;
    for( int row = 0; row < XecuterLcd::ROWS; ++row )
    {
        for( int col = 0; col < XecuterLcd::COLS; ++col )
        {
            unsigned char c = (unsigned char)m_LcdShadow[row][col];
            const unsigned char* glyph = g_LcdFont[( c >= 0x20 && c <= 0x7E ) ? c - 0x20 : 0];
            LONG cx = panelX + PAD_X + col * CELL_W;
            LONG cy = panelY + PAD_Y + row * CELL_H;
            for( int dx = 0; dx < 5; ++dx )
            {
                for( int dy = 0; dy < 8; ++dy )
                {
                    D3DRECT rc = { cx + dx * PITCH, cy + dy * PITCH, cx + dx * PITCH + DOT, cy + dy * PITCH + DOT };
                    if( dy < 7 && ( glyph[dx] & ( 1 << dy ) ) )
                        onRects[onCount++] = rc;
                    else
                        offRects[offCount++] = rc;
                }
            }
        }
    }
    if( offCount ) m_pd3dDevice->Clear( offCount, offRects, D3DCLEAR_TARGET, offColor, 1.0f, 0 );
    if( onCount )  m_pd3dDevice->Clear( onCount,  onRects,  D3DCLEAR_TARGET, onColor,  1.0f, 0 );

    // Footer
    m_Font.Begin();
    if( m_Status[0] && ( m_fAppTime - m_StatusTime ) < 3.0f )
        m_Font.DrawText( 320, 424, 0xff7fe07f, m_Status, XBFONT_CENTER_X );
    else
        m_Font.DrawText( 320, 424, 0xff8fa8c0, L"D-pad: select / adjust     A: re-initialise", XBFONT_CENTER_X );
    m_Font.End();

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    return S_OK;
}

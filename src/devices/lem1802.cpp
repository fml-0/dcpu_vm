#include <devices/lem1802.hpp>

#include <algorithm>

namespace cpu {

namespace lem {


    const uint16_t Lem1802::def_font_map[128*2] = {     /// Default font map
#       include "lem1802_font.inc"
    };

    const uint16_t Lem1802::def_palette_map[16] = {     /// Default palette
#       include "lem1802_palette.inc"
    };

    Lem1802::Lem1802() : screen_map (0), font_map (0), palette_map (0),
    border_col (0), ticks(0), blink(0) { 
        screen.create(Lem1802::WIDTH, Lem1802::HEIGHT, sf::Color::Black);
    }

    Lem1802::~Lem1802() 
    { }

    void Lem1802::attachTo (DCPU* cpu, size_t index) 
    {
        this->IHardware::attachTo(cpu, index);
        blink_max = cpu->cpu_clock / Lem1802::BLINKPERSECOND;
        blink = 0;
        screen_map = font_map, palette_map = 0;
        border_col = 0;
    }

    void Lem1802::handleInterrupt()
    {
        if (this->cpu == NULL)
            return;
        
        size_t s;
        switch (cpu->GetA() ) {
            case MEM_MAP_SCREEN:
                screen_map = cpu->GetB();
                break;

            case MEM_MAP_FONT:
                font_map = cpu->GetB();
                break;

            case MEM_MAP_PALETTE:
                palette_map = cpu->GetB();
                break;

            case SET_BORDER_COLOR:
                border_col = cpu->GetB() & 0xf;
                break;

            case MEM_DUMP_FONT:
                s = RAM_SIZE - 1 - cpu->GetB() < 256 ? 
                        RAM_SIZE - 1 - cpu->GetB() : 256 ;
                std::copy_n (Lem1802::def_font_map,s,cpu->getMem()+cpu->GetB());
                break;

            case MEM_DUMP_PALETTE:
                s = RAM_SIZE - 1 - cpu->GetB() < 16 ?
                        RAM_SIZE - 1 - cpu->GetB() : 16 ;
                std::copy_n (Lem1802::def_palette_map, s, 
                        cpu->getMem() + cpu->GetB() );
                break;

            default:
                // do nothing
                break;
        }
    }

    void Lem1802::tick()
    {
        if (this->cpu == NULL) return;
        if (++ticks > this->cpu->cpu_clock/60) {
            // Update screen at 60Hz aprox
            ticks = 0;
            this->updateScreen();
        }
        if (++blink > blink_max*2)
            blink = 0;
    }

    void Lem1802::updateScreen()
    {
        //TODO use uint32_t to fast copy the color into the buffer
        //TODO remove all multiplications inside loop
        if (this->cpu == NULL || !need_render)
            return;

        need_render = false;
        screen.create(Lem1802::WIDTH, Lem1802::HEIGHT, sf::Color::Black);

        if (screen_map != 0) { // Update the texture
            for (unsigned row=0; row < Lem1802::ROWS; row++) {
                for (unsigned col=0; col < Lem1802::COLS; col++) {
                    uint16_t pos = screen_map + row * Lem1802::COLS + col;
                    unsigned char ascii = (unsigned char) (cpu->getMem()[pos] 
                                                            & 0x007F);
                    // Get palette indexes
                    uint16_t fg_ind = (cpu->getMem()[pos] & 0xF000) >> 12;
                    uint16_t bg_ind = (cpu->getMem()[pos] & 0x0F00) >> 8;
                    uint16_t fg_col, bg_col;

                    if (palette_map == 0) { // Use default palette
                        fg_col = Lem1802::def_palette_map[fg_ind &0xF];
                        bg_col = Lem1802::def_palette_map[bg_ind &0xF];
                    } else {
                        fg_col = cpu->getMem()[palette_map+ (fg_ind &0xF)];
                        bg_col = cpu->getMem()[palette_map+ (bg_ind &0xF)];
                    }
                    
                    // Does the blink
                    if (blink > blink_max &&  
                           ((cpu->getMem()[pos] & 0x80) > 0) ) {
                        fg_col = bg_col;
                    }

                    // Composes RGBA values from palette colors
                    sf::Color fg (
                            ((fg_col & 0xF00)>> 8) *0x11,
                            ((fg_col & 0x0F0)>> 4) *0x11,
                             (fg_col & 0x00F)      *0x11,
                            0xFF );
                    sf::Color bg (
                            ((bg_col & 0xF00)>> 8) *0x11,
                            ((bg_col & 0x0F0)>> 4) *0x11,
                             (bg_col & 0x00F)      *0x11,
                            0xFF );
    
                    uint16_t glyph[2];
                    if (font_map == 0) { // Default font
                        glyph[0] = Lem1802::def_font_map[ascii*2]; 
                        glyph[1] = Lem1802::def_font_map[ascii*2+1];
                    } else {
                        glyph[0] = cpu->getMem()[font_map+ (ascii*2)]; 
                        glyph[1] = cpu->getMem()[font_map+ (ascii*2)+1]; 
                    }
                    
                    for (int i=0; i< 8; i++) { 
                        // *** MSB ***
                        // First word 
                        bool pixel = ((1<<(i+8)) & glyph[0]) > 0;
                        if (pixel) {
                            screen.setPixel (col*4, row*8 +i, fg);
                        } else {
                            screen.setPixel (col*4, row*8 +i, bg);
                        }
                        // Second word
                        pixel = ((1<<(i+8)) & glyph[1]) > 0;
                        if (pixel) {
                            screen.setPixel (col*4 +2, row*8 +i, fg);
                        } else {
                            screen.setPixel (col*4 +2, row*8 +i, bg);
                        }

                        // *** LSB ***
                        // First word 
                        pixel = ((1<<i) & glyph[0]) >0;
                        if (pixel) {
                            screen.setPixel (col*4 +1, row*8 +i, fg);
                        } else {
                            screen.setPixel (col*4 +1, row*8 +i, bg);
                        }
                        // Second word
                        pixel = ((1<<i) & glyph[1]) > 0;
                        if (pixel) {
                            screen.setPixel (col*4 +3, row*8 +i, fg);
                        } else {
                            screen.setPixel (col*4 +3, row*8 +i, bg);
                        }
                    }
                }
            }
        }
    }

    sf::Color Lem1802::getBorder() const
    {
        uint16_t border;
        if (palette_map == 0) { // Use default palette
            border = Lem1802::def_palette_map[border_col];
        } else {
            border = cpu->getMem()[palette_map+ border_col];
        }
        return sf::Color(
                    ((border &0x0F00) >> 8) *0x11 ,
                    ((border &0x00F0) >> 4) *0x11 ,
                    ((border &0x000F) ) *0x11 ,
                    0xFF );
    }

} // END of NAMESPACE lem

} // END of NAMESPACE cpu
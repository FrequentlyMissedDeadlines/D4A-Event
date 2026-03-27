#include "UI/CommandsScreen.h"
#include <TFT_eSPI.h>
extern TFT_eSPI tft;

CommandsScreen::CommandsScreen(AmakerBotService &amakerbot)
    : amakerbot_(amakerbot)
{
}

void CommandsScreen::initScreen()
{

    drawCountersPanel(true);
}

void CommandsScreen::updateScreen()
{
    drawCountersPanel(false);
}

void CommandsScreen::drawCountersPanel(bool chrome_only)
{
    // Panel title
    if (chrome_only)
    {   
        
    }

    // Bot action dispatch counts
    if (!chrome_only)
    {
        
    }
    int line =0;
    int last_action_family = -1;
    char linebuf[40];
    tft.setCursor(0, line++ * 16);
    tft.print("+--------------------------------------+");
    tft.setCursor(0, line++ * 16);
    tft.print("+ Count of calls                       +");
    tft.setCursor(0, line++ * 16);
    tft.print("+--------------------------------------+");
     
    for (size_t action = 0; action < 256; ++action)
        {
            const uint32_t count = amakerbot_.getDispatchCounts()[action] ;

            if (count>0)
            { if ((action >> 4) != last_action_family
                )
                {   
                    snprintf(linebuf, sizeof(linebuf), "+ service %02X +------+----------+", action<<4, action, count);               
                
                    // Print separator between action families
                    tft.setCursor(0, line++ * 16);
                    tft.print(linebuf);
                    last_action_family = action >> 4;
                }
                tft.setCursor(0, line * 16);
                snprintf(linebuf, sizeof(linebuf), "|            | 0x%02X | %8u |", action<<4, action, count);               
                tft.print(linebuf);
                line++;
            }
        }
       
}
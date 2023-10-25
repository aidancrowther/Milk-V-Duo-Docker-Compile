/*******************************************************************************
 * FILENAME: main.c
 *
 * PROJECT:
 *    Bitty HTTP
 *
 * FILE DESCRIPTION:
 *    This is an example main loop.
 *
 * COPYRIGHT:
 *    Copyright (c) 2019 Paul Hutchinson
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a copy
 *    of this software and associated documentation files (the "Software"), to deal
 *    in the Software without restriction, including without limitation the rights
 *    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *    copies of the Software, and to permit persons to whom the Software is
 *    furnished to do so, subject to the following conditions:
 *    
 *    The above copyright notice and this permission notice shall be included in all
 *    copies or substantial portions of the Software.
 *    
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *    SOFTWARE.
 *
 ******************************************************************************/

/*** HEADER FILES TO INCLUDE  ***/
#include "main.h"
#include "WebServer.h"
#include <stdint.h>
#include <stdio.h>

#include <unistd.h> // usleep()

/*** DEFINES                  ***/

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/

/*** FUNCTION PROTOTYPES      ***/

/*** VARIABLE DEFINITIONS     ***/
bool g_Quit;

int main(void)
{
    t_ElapsedTime Waiting2End;

    SocketsCon_InitSocketConSystem();
    WS_Init();

    if(!WS_Start(3000))
    {
        printf("Failed to start web server\n");
        return 0;
    }

    printf("Waiting for connections on port 3000\n");

    g_Quit=false;
    while(!g_Quit)
    {
        WS_Tick();
        usleep(1000);
    }

    printf("Quiting...\n");

    /* Run the web server for a while so we can send any "finished" page */
    Waiting2End=ReadElapsedClock();
    while(ReadElapsedClock()-Waiting2End<3)
        WS_Tick();

    WS_Shutdown();
    SocketsCon_ShutdownSocketConSystem();

    return 0;
}

/*******************************************************************************
 * NAME:
 *    ReadElapsedClock
 *
 * SYNOPSIS:
 *    t_ElapsedTime ReadElapsedClock(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    This function reads a system clock (in seconds) and can be used for
 *    for elapsed time readings (basicly timers where you store the start
 *    time and then subtract the current value and see if it bigger than
 *    your timeout).
 *
 *    There is no need for this to be a real time clock, just something that
 *    counts up.
 *
 * RETURNS:
 *    The current clock time in seconds.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
t_ElapsedTime ReadElapsedClock(void)
{
    time_t Current; // The current time

    Current=time(NULL);

    return (uint32_t)Current;
}

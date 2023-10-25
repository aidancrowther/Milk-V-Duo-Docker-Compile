/*******************************************************************************
 * FILENAME: Options.h
 * 
 * PROJECT:
 *    Bitty HTTP
 *
 * FILE DESCRIPTION:
 *    This file has things that you might want to change to config the web
 *    server in it.
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
 *******************************************************************************/
#ifndef __OPTIONS_H_
#define __OPTIONS_H_

/***  HEADER FILES TO INCLUDE          ***/

/***  DEFINES                          ***/
#define DOCVER                              "1.0.0.0"

#define WS_OPT_MAX_CONNECTIONS              16      // The max number of connections we can handle at the same time (this will include buffers needed for each connection)
#define WS_OPT_ARG_MEMORY_SIZE              100     // The memory block to use to store the cookies, get args, and post args
#define WS_SECONDS_UNTIL_CONNECTION_RELEASE 10      // How many seconds to wait after a connection stops sending to us before we hang up
#define WS_LINE_BUFFER_SIZE                 256     // The max number of bytes we can handle a single header line can be (including the GET line).  This is normally in the order of 16K - 128K (we default to a lot less)

/***  MACROS                           ***/

/***  TYPE DEFINITIONS                 ***/

/***  CLASS DEFINITIONS                ***/

/***  GLOBAL VARIABLE DEFINITIONS      ***/

/***  EXTERNAL FUNCTION PROTOTYPES     ***/

#endif

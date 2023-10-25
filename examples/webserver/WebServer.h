/*******************************************************************************
 * FILENAME: WebServer.h
 * 
 * PROJECT:
 *    Bitty HTTP
 *
 * FILE DESCRIPTION:
 *    This is the web server's main .h file.  You need to copy this file to
 *    your project.
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
#ifndef __WEBSERVER_H_
#define __WEBSERVER_H_

/***  HEADER FILES TO INCLUDE          ***/
#include "SocketsCon.h"
#include "Options.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/***  DEFINES                          ***/

/***  MACROS                           ***/

/***  TYPE DEFINITIONS                 ***/
typedef enum
{
    e_ReplyStatus_Ok,                           // 200
    e_ReplyStatus_MovedPerm,                    // 301
    e_ReplyStatus_NotModified,                  // 304
    e_ReplyStatus_TmpRedirect,                  // 307
    e_ReplyStatus_PermRedirect,                 // 308
    e_ReplyStatus_BadRequest,                   // 400
    e_ReplyStatus_Forbidden,                    // 403
    e_ReplyStatus_NotFound,                     // 404
    e_ReplyStatus_MethodNotAllowed,             // 405
    e_ReplyStatus_URITooLong,                   // 414
    e_ReplyStatus_RequestHeaderFieldsTooLarge,  // 431
    e_ReplyStatus_InternalServerError,          // 500
    e_ReplyStatus_NotImplemented,               // 501
    e_ReplyStatus_HTTPVersionNotSupported,      // 505
    e_ReplyStatus_InsufficientStorage,          // 507
    e_ReplyStatusMAX
} e_ReplyStatusType;

typedef enum
{
    e_WebServerState_Closed,
    e_WebServerState_Request,
    e_WebServerState_Headers,
    e_WebServerState_Body,
    e_WebServerState_Response,
    e_WebServerStateMAX
} e_WebServerStateType;

typedef enum
{
    e_ReqType_Get,
    e_ReqType_Post,
    e_ReqTypeMAX
} e_ReqTypeType;

struct WSPageProp
{
    bool DynamicFile;
    const char **Cookies;
    const char **Gets;
    const char **Posts;
    uintptr_t FileID;
};

typedef enum
{
    e_WSPostState_GettingKey,
    e_WSPostState_GettingValue,
    e_WSPostState_Error,
    e_WSPostStateMAX
} e_WSPostStateType;

typedef uint32_t t_ElapsedTime;   // Time to be used for elapsed time

struct WebServer
{
    e_WebServerStateType State;
    struct SocketCon Con;
    int LineBuffPos;
    char LineBuff[WS_LINE_BUFFER_SIZE];
    e_ReqTypeType Req;
    e_ReplyStatusType ReplyStatus;
    bool UserSetReplyStatus;
    bool WriteStarted;
    bool WriteChunked;
    bool ReplyStarted;
    struct WSPageProp PageProp;
    t_ElapsedTime LastReadTime;
    uint32_t BodySize;
    e_WSPostStateType PostState;
    char *PostWritePos;
    char *PostEndOfStorage;
    char ArgsStorage[WS_OPT_ARG_MEMORY_SIZE];
};

/***  CLASS DEFINITIONS                ***/

/***  GLOBAL VARIABLE DEFINITIONS      ***/

/***  EXTERNAL FUNCTION PROTOTYPES     ***/
void WS_Init(void);
void WS_Shutdown(void);
bool WS_Start(uint16_t Port);
void WS_Tick(void);
void WS_WriteWhole(struct WebServer *Web,const char *Buffer,int Len);
void WS_WriteWholeStr(struct WebServer *Web,const char *Buffer);
void WS_WriteChunk(struct WebServer *Web,const char *Buffer,int Len);
void WS_WriteChunkStr(struct WebServer *Web,const char *Buffer);
bool WS_Header(struct WebServer *Web,const char *Header);
bool WS_Location(struct WebServer *Web,const char *NewURL);
bool WS_SetHTTPStatusCode(struct WebServer *Web,e_ReplyStatusType Code);
const char *WS_GET(struct WebServer *Web,const char *Arg);
const char *WS_COOKIE(struct WebServer *Web,const char *Arg);
const char *WS_POST(struct WebServer *Web,const char *Arg);
bool WS_SetCookie(struct WebServer *Web,const char *Name,const char *Value,
        time_t Expire,const char *Path,const char *Domain,bool Secure,
        bool HttpOnly);
bool WS_URLEncode(const char *Value,char *OutputBuffer,int MaxLen);
bool WS_URLDecode(const char *Value,char *Decoded,int MaxLen);
char *WS_URLDecodeInPlace(char *Value);
int WS_GetOSSocketHandles(t_ConSocketHandle *Handles);

/* Web server calls these */
bool FS_GetFileProperties(const char *Filename,struct WSPageProp *PageProp);
void FS_SendFile(struct WebServer *Web,uintptr_t FileID);
t_ElapsedTime ReadElapsedClock(void);

#endif

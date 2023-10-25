/*******************************************************************************
 * FILENAME: WebServer.c
 *
 * PROJECT:
 *    Bitty HTTP 1.4
 *
 * FILE DESCRIPTION:
 *    This file has the main web server in it.  You need to copy this file
 *    to your project.
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
#include "WebServer.h"
#include "SocketsCon.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*** DEFINES                  ***/

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/

/*** FUNCTION PROTOTYPES      ***/
static int WS_GetNextLine(struct WebServer *Web,char *ReadBuff,int Bytes);
static void WS_RunServer(struct WebServer *Web,char *ReadBuff,int Bytes);
static bool WS_ProcessURI(struct WebServer *Web);
static void WS_ProcessGetVars(struct WebServer *Web);
static void WS_ProcessCookieVars(struct WebServer *Web);
static void WS_ProcessHeader(struct WebServer *Web);
static void WS_SendResponse(struct WebServer *Web);
static void WS_ProcessETag(struct WebServer *Web,bool Weak,const char *ETag);
static void WS_ResetWebServer(struct WebServer *Web);
static void WS_EndReply(struct WebServer *Web);
static char *WS_SkipStorageArgs(char *StartingPos,const char **ArgsList);
static const char *WS_FindArgInStorage(char *Pos,const char *Arg,
        const char **ArgsList);
static void WS_StartReply(struct WebServer *Web);
static void WS_StartProcessingPOSTVar(struct WebServer *Web);
static bool WS_CopyLineBuffer2POSTVar(struct WebServer *Web);
static void WS_InsertCopy(char *Dest,char *DestEnd,const char *Src,int CopyLen);
//static void DEBUG_PrintStoredArgs(struct WebServer *Web);

/*** VARIABLE DEFINITIONS     ***/
struct SocketCon m_ListeningSocket;
struct WebServer m_WebServers[WS_OPT_MAX_CONNECTIONS];

/*******************************************************************************
 * NAME:
 *    WS_Init
 *
 * SYNOPSIS:
 *    void WS_Init(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    This function init's the web server.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    WS_Shutdown()
 ******************************************************************************/
void WS_Init(void)
{
    int r;

    SocketsCon_InitSockCon(&m_ListeningSocket);
    for(r=0;r<WS_OPT_MAX_CONNECTIONS;r++)
        SocketsCon_InitSockCon(&m_WebServers[r].Con);
}

/*******************************************************************************
 * NAME:
 *    WS_Shutdown
 *
 * SYNOPSIS:
 *    void WS_Shutdown(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    This function releases anything the web server is using.  After you
 *    call this you need to call WS_Init() again before you can use any
 *    web server functions.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    WS_Init()
 ******************************************************************************/
void WS_Shutdown(void)
{
    int r;

    SocketsCon_Close(&m_ListeningSocket);
    for(r=0;r<WS_OPT_MAX_CONNECTIONS;r++)
        SocketsCon_Close(&m_WebServers[r].Con);
}

/*******************************************************************************
 * NAME:
 *    WS_Start
 *
 * SYNOPSIS:
 *    bool WS_Start(uint16_t Port);
 *
 * PARAMETERS:
 *    Port [I] -- What port to listen on
 *
 * FUNCTION:
 *    This function starts the web server listening for incoming connections.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error
 *
 * SEE ALSO:
 *    WS_WriteWhole(), WS_WriteChunk(), WS_Header(),
 ******************************************************************************/
bool WS_Start(uint16_t Port)
{
    SocketsCon_EnableAddressReuse(&m_ListeningSocket,true);

    if(!SocketsCon_Listen(&m_ListeningSocket,NULL,Port))
        return false;
    return true;
}

/*******************************************************************************
 * NAME:
 *    WS_ResetWebServer
 *
 * SYNOPSIS:
 *    static void WS_ResetWebServer(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function resets a web server context to defaults.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_ResetWebServer(struct WebServer *Web)
{
    Web->LineBuffPos=0;
    Web->State=e_WebServerState_Request;
    Web->ReplyStatus=e_ReplyStatusMAX;
    Web->UserSetReplyStatus=false;
    Web->WriteStarted=false;
    Web->WriteChunked=false;
    Web->PageProp.DynamicFile=false;
    Web->PageProp.Cookies=NULL;
    Web->PageProp.Gets=NULL;
    Web->PageProp.Posts=NULL;
    Web->ReplyStarted=false;
    Web->LastReadTime=ReadElapsedClock();
    Web->BodySize=0;
    Web->PostState=e_WSPostState_GettingKey;
    Web->PostWritePos=NULL;
    Web->PostEndOfStorage=NULL;
}

/*******************************************************************************
 * NAME:
 *    WS_Tick
 *
 * SYNOPSIS:
 *    void WS_Tick(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    This function runs the web server.  It accepts new connections, reads
 *    from existing connections, and sends data out of the open connections
 *    (it also handles timeouts).
 *
 * RETURNS:
 *    NONE
 *
 * NOTES:
 *    This must be called regularly.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void WS_Tick(void)
{
    int con;
    int Bytes;
    char ReadBuff[100];

    SocketsCon_Tick(&m_ListeningSocket);

    /* Do all the connections */
    for(con=0;con<WS_OPT_MAX_CONNECTIONS;con++)
    {
        SocketsCon_Tick(&m_WebServers[con].Con);

        if(!SocketsCon_IsConnected(&m_WebServers[con].Con))
        {
            /* Poll for any new connections (we keep asking giving each free
               connection a chance to get the new connection) */
            if(SocketsCon_Accept(&m_ListeningSocket,&m_WebServers[con].Con))
            {
                /* Ok, we got a new connection */
                WS_ResetWebServer(&m_WebServers[con]);
            }
            else
            {
                if(SocketsCon_GetErrorCode(&m_ListeningSocket)!=
                        e_ConnectError_AllOk)
                {
                    /* We had an error accepting the connection, the listening
                       socket it now closed */
                }
            }
        }
        else
        {
            /* Handle requests from connected connections */
            Bytes=SocketsCon_Read(&m_WebServers[con].Con,ReadBuff,
                    sizeof(ReadBuff));
            if(Bytes==0)
            {

                if(ReadElapsedClock()-m_WebServers[con].LastReadTime>=
                        WS_SECONDS_UNTIL_CONNECTION_RELEASE)
                {
                    /* Ok, connection timed out, hang up so others can use it */
                    SocketsCon_Close(&m_WebServers[con].Con);
                    m_WebServers[con].State=e_WebServerState_Closed;
                }

                continue;
            }
            if(Bytes<0)
            {
                /* Error, hang up */
                SocketsCon_Close(&m_WebServers[con].Con);
                m_WebServers[con].State=e_WebServerState_Closed;
                continue;
            }

            WS_RunServer(&m_WebServers[con],ReadBuff,Bytes);

            m_WebServers[con].LastReadTime=ReadElapsedClock();
        }
    }
}

/*******************************************************************************
 * NAME:
 *    WS_RunServer
 *
 * SYNOPSIS:
 *    static void WS_RunServer(struct WebServer *Web,char *ReadBuff,int Bytes);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    ReadBuff [I] -- The new bytes that have been read from this connection.
 *    Bytes [I] -- The number of bytes read.
 *
 * FUNCTION:
 *    This function handles the http headers and other data coming from the
 *    web browser.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_RunServer(struct WebServer *Web,char *ReadBuff,int Bytes)
{
    int BytesUsed;
    char *ReadPoint;
    int BytesLeft;

    ReadPoint=ReadBuff;
    BytesLeft=Bytes;
    for(;;)
    {
        switch(Web->State)
        {
            case e_WebServerState_Closed:
                SocketsCon_Close(&Web->Con);
                return;
            break;
            case e_WebServerState_Request:
                BytesUsed=WS_GetNextLine(Web,ReadPoint,BytesLeft);
                if(BytesUsed==0)
                    return;
                if(BytesUsed<0)
                {
                    Web->ReplyStatus=e_ReplyStatus_URITooLong;
                    WS_StartReply(Web);
                    WS_EndReply(Web);
                    SocketsCon_Close(&Web->Con);
                    Web->State=e_WebServerState_Closed;
                    return;
                }

                ReadPoint+=BytesUsed;
                BytesLeft-=BytesUsed;

                /* We found the end */
                if(strncmp(Web->LineBuff,"GET ",4)==0)
                {

                    Web->Req=e_ReqType_Get;
                    WS_ProcessURI(Web);
                    if(FS_GetFileProperties(&Web->LineBuff[4],&Web->PageProp))
                    {
                        WS_ProcessGetVars(Web);
                    }
                    else
                    {
                        Web->ReplyStatus=e_ReplyStatus_NotFound;
                    }
                }
                else if(strncmp(Web->LineBuff,"POST ",5)==0)
                {
                    Web->Req=e_ReqType_Post;
                    WS_ProcessURI(Web);
                    if(FS_GetFileProperties(&Web->LineBuff[5],&Web->PageProp))
                    {
                        WS_ProcessGetVars(Web);
                    }
                    else
                    {
                        Web->ReplyStatus=e_ReplyStatus_NotFound;
                    }
                }
                else
                {
                    Web->ReplyStatus=e_ReplyStatus_NotImplemented;
                }
                Web->State++;
            break;
            case e_WebServerState_Headers:
                BytesUsed=WS_GetNextLine(Web,ReadPoint,BytesLeft);
                if(BytesUsed==0)
                    return;
                if(BytesUsed<0)
                {
                    Web->ReplyStatus=e_ReplyStatus_RequestHeaderFieldsTooLarge;
                    WS_StartReply(Web);
                    WS_EndReply(Web);
                    SocketsCon_Close(&Web->Con);
                    Web->State=e_WebServerState_Closed;
                    continue;
                }

                ReadPoint+=BytesUsed;
                BytesLeft-=BytesUsed;

                if(Web->LineBuff[0]==0)
                    Web->State++;
                else
                    WS_ProcessHeader(Web);
            break;
            case e_WebServerState_Body:
                /* We need to read in the whole body before moving on */
                if(Web->Req==e_ReqType_Post)
                {
                    while(BytesLeft>0)
                    {
                        switch(Web->PostState)
                        {
                            case e_WSPostState_GettingKey:
                                if(*ReadPoint=='=')
                                {
                                    /* We are at the end of the name of the
                                       POST var.  See if we can find it in
                                       the list of POST vars we are
                                       expecting. */
                                    Web->LineBuff[Web->LineBuffPos++]=0;

                                    /* Decode the key */
                                    WS_URLDecodeInPlace(Web->LineBuff);

                                    /* Setup for starting to store this var */
                                    WS_StartProcessingPOSTVar(Web);

                                    Web->PostState=e_WSPostState_GettingValue;
                                    Web->LineBuffPos=0;
                                    break;
                                }
                                Web->LineBuff[Web->LineBuffPos++]=*ReadPoint;
                                if(Web->LineBuffPos>=sizeof(Web->LineBuff)-1)
                                {
                                    /* Out of space */
                                    Web->ReplyStatus=
                                            e_ReplyStatus_InsufficientStorage;
                                    Web->PostState=e_WSPostState_Error;
                                    break;
                                }
                            break;
                            case e_WSPostState_GettingValue:
                                if(*ReadPoint=='&')
                                {
                                    /* Ok, this is the end of the var, finish
                                       copying */
                                    if(!WS_CopyLineBuffer2POSTVar(Web))
                                    {
                                        Web->PostState=e_WSPostState_Error;
                                        break;
                                    }

                                    Web->PostState=e_WSPostState_GettingKey;
                                }
                                else
                                {
                                    /* Store as much as we can in the
                                       Line Buffer.  When it fills we copy it
                                       over to the arg storage. */
                                    Web->LineBuff[Web->LineBuffPos++]=
                                            *ReadPoint;
                                    if(Web->LineBuffPos>=sizeof(Web->LineBuff)-1)
                                    {
                                        /* Line buffer filled.  Empty it */
                                        if(!WS_CopyLineBuffer2POSTVar(Web))
                                        {
                                            Web->PostState=e_WSPostState_Error;
                                            break;
                                        }
                                    }
                                }
                            break;
                            case e_WSPostState_Error:
                                /* Skip until we get to the next var */
                                if(*ReadPoint=='&')
                                {
                                    Web->PostState=e_WSPostState_GettingKey;
                                    Web->LineBuffPos=0; // Setup for next var
                                }
                            break;
                            case e_WSPostStateMAX:
                            break;
                        }

                        /* Consume this char */
                        ReadPoint++;
                        BytesLeft--;
                        Web->BodySize--;
                        if(Web->BodySize==0)
                        {
                            /* Ran out of body */
                            if(Web->PostState==e_WSPostState_GettingValue)
                            {
                                /* Finish processing the POST var */
                                if(!WS_CopyLineBuffer2POSTVar(Web))
                                {
                                    Web->PostState=e_WSPostState_Error;
                                    break;
                                }

                                Web->PostState=e_WSPostState_GettingKey;
                                Web->LineBuffPos=0; // Setup for next var
                            }
                        }
                    }
                }
                else
                {
                    if(Web->BodySize<BytesLeft)
                    {
                        BytesUsed=BytesLeft-Web->BodySize;
                        Web->BodySize=0;
                    }
                    else
                    {
                        Web->BodySize-=BytesLeft;
                        BytesUsed=BytesLeft;
                    }

                    /* Use up the bytes */
                    ReadPoint+=BytesUsed;
                    BytesLeft-=BytesUsed;
                }

                if(Web->BodySize==0)
                {
                    /* Ok, we have read all of the body */
                    Web->State++;
                }
                else
                {
                    /* We need more bytes to finish the body */
                    return;
                }
            break;
            case e_WebServerState_Response:
//DEBUG_PrintStoredArgs(Web);
                WS_SendResponse(Web);
                WS_ResetWebServer(Web);
                return;
            break;
            case e_WebServerStateMAX:
            break;
        }
    }
}

/*******************************************************************************
 * NAME:
 *    WS_GetNextLine
 *
 * SYNOPSIS:
 *    static int WS_GetNextLine(struct WebServer *Web,char *ReadBuff,int Bytes);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    ReadBuff [I] -- The new bytes read
 *    Bytes [I] -- The number of bytes read
 *
 * FUNCTION:
 *    This function collects bytes from the connection until a header line
 *    from the http connection is fully read.
 *
 * RETURNS:
 *    The number of bytes used out of the 'ReadBuff' (how many bytes to advance
 *    the read point)
 *    -1 -- Out of space
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static int WS_GetNextLine(struct WebServer *Web,char *ReadBuff,int Bytes)
{
    int r;

    for(r=0;r<Bytes;r++)
    {
        if(ReadBuff[r]=='\r')
            continue;

        if(ReadBuff[r]=='\n')
        {
            /* We are at the end */
            Web->LineBuff[Web->LineBuffPos++]=0;    // String it
            Web->LineBuffPos=0; // Setup for next line
            break;
        }
        Web->LineBuff[Web->LineBuffPos++]=ReadBuff[r];
        if(Web->LineBuffPos>=sizeof(Web->LineBuff))
        {
            /* Out of space */
            Web->LineBuffPos=0; // Setup for next line
            return -1;
        }
    }
    if(r<Bytes)
        return r+1;

    return 0;
}

/*******************************************************************************
 * NAME:
 *    WS_ProcessURI
 *
 * SYNOPSIS:
 *    static bool WS_ProcessURI(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function takes the line buffer from 'Web' and processes the URI
 *    line (the first line in the http connection before the headers).
 *
 * RETURNS:
 *    true -- All ok
 *    false -- There was an error, the reply status has been set.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static bool WS_ProcessURI(struct WebServer *Web)
{
    int EndOfLine;
    char *ArgsStart;

    EndOfLine=strlen(Web->LineBuff);

    /* Find the version */
    while(EndOfLine>0)
    {
        if(Web->LineBuff[EndOfLine]=='H')
            break;
        EndOfLine--;
    }
    if(EndOfLine==0)
    {
        /* Bad request */
        Web->ReplyStatus=e_ReplyStatus_BadRequest;
        return false;
    }
    if(strcmp(&Web->LineBuff[EndOfLine],"HTTP/1.1")!=0)
    {
        /* We only support 1.1 */
        Web->ReplyStatus=e_ReplyStatus_HTTPVersionNotSupported;
        return false;
    }
    EndOfLine--;
    Web->LineBuff[EndOfLine]=0;

    /* Find the get args (if any) */
    ArgsStart=Web->LineBuff;
    while(*ArgsStart!='?' && *ArgsStart!=0)
        ArgsStart++;

    if(*ArgsStart!=0)
    {
        *ArgsStart=0;   // Blank the '?'
    }
    else
    {
        /* Add a second '\0' to the end (to end the args, we will be
           overwriting the 'H' in HTTP) */
        ArgsStart++;
        *ArgsStart=0;
    }

    return true;
}

/*******************************************************************************
 * NAME:
 *    WS_ProcessHeader
 *
 * SYNOPSIS:
 *    static void WS_ProcessHeader(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function processes a http header line.  It will take the line from
 *    'Web->LineBuff'.
 *
 *    Currently supported headers:
 *      Cookie -- Used for sending a cookie back to the server
 *      If-None-Match -- Used for ETag caching.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_ProcessHeader(struct WebServer *Web)
{
    char *ETag;
    char *End;
    char *Pos;
    bool Weak;

    if(strncmp(Web->LineBuff,"If-None-Match:",14)==0)
    {
        /* Ok, check the ETag */
        ETag=&Web->LineBuff[14];
        while(*ETag==' ')
            ETag++;

        while(*ETag!=0)
        {
            if(*ETag=='*')
            {
                /* Match everything? */
                return;
            }

            Weak=false;
            if(strncmp(ETag,"W/",2)==0)
                Weak=true;

            /* Find the start of the next tag */
            while(*ETag!='\"' && *ETag!=0)
                ETag++;
            if(*ETag==0)
            {
                Web->ReplyStatus=e_ReplyStatus_BadRequest;
                return;
            }

            /* Ok, skip the " and find the end */
            ETag++;
            End=ETag;
            while(*End!='\"' && *End!=0)
                End++;
            if(*End==0)
            {
                /* The end? */
                Web->ReplyStatus=e_ReplyStatus_BadRequest;
                return;
            }
            *End=0;
            WS_ProcessETag(Web,Weak,ETag);
            /* Ok, Move to just after the " */
            ETag=End+1;

            /* Skip until we find a , or the end */
            while(*ETag!=',' && *ETag!=0)
                ETag++;
        }
    }
    if(strncmp(Web->LineBuff,"Cookie:",7)==0)
    {
        /* We have a cookie */
        WS_ProcessCookieVars(Web);
    }
    if(strncmp(Web->LineBuff,"Content-Length:",15)==0)
    {
        Pos=&Web->LineBuff[15];
        if(*Pos==' ')
            Pos++;
        Web->BodySize=strtol(Pos,NULL,10);
    }
}

/*******************************************************************************
 * NAME:
 *    WS_ProcessETag
 *
 * SYNOPSIS:
 *    static void WS_ProcessETag(struct WebServer *Web,bool Weak,
 *          const char *ETag);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Weak [I] -- Is this ETag a weak tag (ignored)
 *    ETag [I] -- The ETag string.
 *
 * FUNCTION:
 *    This function checks to see if the ETag for a request matches the
 *    doc version (and that the page is not marked as dynamic).
 *    If the doc version matches it send back that the page has not
 *    been changed (browser should use local version).
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_ProcessETag(struct WebServer *Web,bool Weak,const char *ETag)
{
    if(!Web->PageProp.DynamicFile && strcmp(ETag,DOCVER)==0)
    {
        /* They match, we are going to reply with HTTP code */
        Web->ReplyStatus=e_ReplyStatus_NotModified;
    }
}

/*******************************************************************************
 * NAME:
 *    WS_StartReply
 *
 * SYNOPSIS:
 *    static void WS_StartReply(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function is called to start a reply.  This means you can not
 *    change the reply status after this function has been called.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_StartReply(struct WebServer *Web)
{
    const char *Msg;
    char buff[100];

    SocketsCon_Write(&Web->Con,"HTTP/1.1 ",9);

    switch(Web->ReplyStatus)
    {
        case e_ReplyStatus_Ok:
            Msg="200 OK";
        break;
        case e_ReplyStatus_MovedPerm:
            Msg="301 Moved Permanently";
        break;
        case e_ReplyStatus_NotModified:
            Msg="304 Not Modified";
        break;
        case e_ReplyStatus_TmpRedirect:
            Msg="307 Temporary Redirect";
        break;
        case e_ReplyStatus_PermRedirect:
            Msg="308 Permanent Redirect";
        break;
        case e_ReplyStatus_BadRequest:
            Msg="400 Bad Request";
        break;
        case e_ReplyStatus_Forbidden:
            Msg="403 Forbidden";
        break;
        case e_ReplyStatus_NotFound:
            Msg="404 Not Found";
        break;
        case e_ReplyStatus_MethodNotAllowed:
            Msg="405 Method Not Allowed";
        break;
        case e_ReplyStatus_URITooLong:
            Msg="414 URI Too Long";
        break;
        case e_ReplyStatus_RequestHeaderFieldsTooLarge:
            Msg="431 Request Header Fields Too Large";
        break;
        case e_ReplyStatusMAX:
        case e_ReplyStatus_InternalServerError:
            Msg="500 Internal Server Error";
        break;
        case e_ReplyStatus_NotImplemented:
            Msg="501 Not Implemented";
        break;
        case e_ReplyStatus_HTTPVersionNotSupported:
            Msg="505 HTTP Version Not Supported";
        break;
        case e_ReplyStatus_InsufficientStorage:
            Msg="507 Insufficient Storage";
        break;
    }

    SocketsCon_Write(&Web->Con,Msg,strlen(Msg));
    SocketsCon_Write(&Web->Con,"\r\n",2);

    SocketsCon_Write(&Web->Con,"Server: BittyHTTP\r\n",19);

    if(Web->ReplyStatus!=e_ReplyStatus_Ok && !Web->UserSetReplyStatus)
    {
        sprintf(buff,"Content-Length: %zd\r\n",strlen(Msg));
        SocketsCon_Write(&Web->Con,buff,strlen(buff));
        SocketsCon_Write(&Web->Con,"\r\n",2);
        SocketsCon_Write(&Web->Con,Msg,strlen(Msg));
    }
    else
    {
        if(!Web->PageProp.DynamicFile)
        {
            /* It's dynamic, so we need to add the ETag */
            sprintf(buff,"ETag: \"%s\"\r\n",DOCVER);
            SocketsCon_Write(&Web->Con,buff,strlen(buff));
        }
    }

    Web->ReplyStarted=true;
}

/*******************************************************************************
 * NAME:
 *    WS_EndReply
 *
 * SYNOPSIS:
 *    static void WS_EndReply(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function ends the reply headers and starts the content for the
 *    current request.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_EndReply(struct WebServer *Web)
{
    if(Web->WriteChunked)
    {
        SocketsCon_Write(&Web->Con,"0\r\n",3);
        SocketsCon_Write(&Web->Con,"\r\n",2);   // No extra header fields
    }
}

/*******************************************************************************
 * NAME:
 *    WS_SendResponse
 *
 * SYNOPSIS:
 *    static void WS_SendResponse(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function does the callback into the file server when the web server
 *    is ready to start sending the contents.
 *
 *    It may not call the file server if there was an error so there is no
 *    content to send.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_SendResponse(struct WebServer *Web)
{
/* DEBUG PAUL: Let the code handle e_ReplyStatus_InsufficientStorage and
   override it if it wants */
    if(Web->ReplyStatus==e_ReplyStatusMAX)
    {
        /* Ok, process file */
        Web->ReplyStatus=e_ReplyStatus_Ok;
        FS_SendFile(Web,Web->PageProp.FileID);
    }
    else
    {
        WS_StartReply(Web);
    }
    WS_EndReply(Web);
}

/*******************************************************************************
 * NAME:
 *    WS_WriteWhole
 *
 * SYNOPSIS:
 *    void WS_WriteWhole(struct WebServer *Web,const char *Buffer,int Len);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Buffer [I] -- The buffer with the whole contents to send in it.
 *    Len [I] -- The number of bytes in 'Buffer'
 *
 * FUNCTION:
 *    This function sends the content to the client using 'Content-Length'.
 *    After you call this function you can not send any more content or headers.
 *
 *    You use this function if you are sending a static resource like a graphic
 *    or static web page.  You can also use this you have built all the content
 *    is a buffer and will not need to send any more.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    WS_Start(), WS_WriteWholeStr(), WS_WriteChunk()
 ******************************************************************************/
void WS_WriteWhole(struct WebServer *Web,const char *Buffer,int Len)
{
    char buff[100];

    if(Web->WriteStarted)
    {
        Web->ReplyStatus=e_ReplyStatus_InternalServerError;
        return;
    }

    Web->WriteStarted=true;
    Web->ReplyStatus=e_ReplyStatus_Ok;

    if(!Web->ReplyStarted)
        WS_StartReply(Web);

    sprintf(buff,"Content-Length: %d\r\n",Len);
    SocketsCon_Write(&Web->Con,buff,strlen(buff));
    SocketsCon_Write(&Web->Con,"\r\n",2);
    SocketsCon_Write(&Web->Con,Buffer,Len);
}

/*******************************************************************************
 * NAME:
 *    WS_WriteChunk
 *
 * SYNOPSIS:
 *    void WS_WriteChunk(struct WebServer *Web,const char *Buffer,int Len);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Buffer [I] -- The buffer with the contents to send in it.
 *    Len [I] -- The number of bytes in 'Buffer'
 *
 * FUNCTION:
 *    This function sends content using the chunked method.  This means you
 *    do not need to know the length of the content before sending (you can
 *    build it as you go).
 *
 *    You would normally use this function for dynamic content.  That way
 *    you do not have to build the whole web page in a buffer before
 *    repling.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    WS_WriteChunkStr(), WS_WriteWhole()
 ******************************************************************************/
void WS_WriteChunk(struct WebServer *Web,const char *Buffer,int Len)
{
    char buff[100];

    if(Len==0)
        return;

    if(!Web->WriteStarted)
    {
        WS_StartReply(Web);
        SocketsCon_Write(&Web->Con,"Transfer-Encoding: chunked\r\n",28);
        /* End the headers */
        SocketsCon_Write(&Web->Con,"\r\n",2);
    }
    Web->WriteChunked=true;
    Web->WriteStarted=true;

    sprintf(buff,"%X\r\n",Len);
    SocketsCon_Write(&Web->Con,buff,strlen(buff));
    SocketsCon_Write(&Web->Con,Buffer,Len);
    SocketsCon_Write(&Web->Con,"\r\n",2);   // End of chunk
}

/*******************************************************************************
 * NAME:
 *    WS_WriteWholeStr
 *
 * SYNOPSIS:
 *    void WS_WriteWholeStr(struct WebServer *Web,const char *Buffer);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Buffer [I] -- The buffer with the whole contents to send in it.  This
 *                  is a C string (ending in \0).
 *
 * FUNCTION:
 *    This function sends the content to the client using 'Content-Length'.
 *    After you call this function you can not send any more content or headers.
 *
 *    This is the same as WS_WriteWhole() except it works on a C string
 *    instead of a buffer.  This function just calls WS_WriteWhole() with
 *    a strlen() for the length.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    WS_Start(), WS_WriteWhole()
 ******************************************************************************/
void WS_WriteWholeStr(struct WebServer *Web,const char *Buffer)
{
    WS_WriteWhole(Web,Buffer,strlen(Buffer));
}

/*******************************************************************************
 * NAME:
 *    WS_WriteChunkStr
 *
 * SYNOPSIS:
 *    void WS_WriteChunkStr(struct WebServer *Web,const char *Buffer);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Buffer [I] -- The buffer with the contents to send in it.  This is
 *                  a C string (ending in \0).
 *
 * FUNCTION:
 *    This function sends content using the chunked method.  This means you
 *    do not need to know the length of the content before sending (you can
 *    build it as you go).
 *
 *    This is the same as WS_WriteChunk() except it works on a C string
 *    instead of a buffer.  This function just calls WS_WriteChunk() with
 *    a strlen() for the length.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    WS_Start(), WS_WriteChunk()
 ******************************************************************************/
void WS_WriteChunkStr(struct WebServer *Web,const char *Buffer)
{
    WS_WriteChunk(Web,Buffer,strlen(Buffer));
}

/*******************************************************************************
 * NAME:
 *    WS_URLDecode
 *
 * SYNOPSIS:
 *    bool WS_URLDecode(const char *Value,char *Decoded,int MaxLen);
 *
 * PARAMETERS:
 *    Value [I] -- The string to decode.
 *    Decoded [O] -- The buffer to store the decoded string in.
 *    MaxLen [I] -- The size of 'Decoded'.
 *
 * FUNCTION:
 *    This function encodes a string into URL encoding (%20 for space)
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- If we had to clip the string to keep it in 'MaxLen'.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
bool WS_URLDecode(const char *Value,char *Decoded,int MaxLen)
{
    char *Write;
    const char *Read;
    char buff[100];

    Write=Decoded;
    Read=Value;
    while(*Read!=0)
    {
        if(*Read=='%')
        {
            /* Encoded */
            Read++; // Move past the %

            /* Copy the next 2 bytes */
            buff[0]=*Read++;
            buff[1]=*Read++;
            buff[2]=0;

            *Write++=strtol(buff,NULL,16);
            if(Write-Decoded>=MaxLen)
                return false;
        }
        else
        {
            *Write++=*Read;
            if(Write-Decoded>=MaxLen)
                return false;
            Read++;
        }
    }
    *Write=0;
    return true;
}

/*******************************************************************************
 * NAME:
 *    WS_URLDecodeInPlace
 *
 * SYNOPSIS:
 *    char *WS_URLDecodeInPlace(char *Value);
 *
 * PARAMETERS:
 *    Value [I] -- The URL encoded string to decode
 *
 * FUNCTION:
 *    This function decodes a URL encoded string directly over top of the
 *    string.
 *
 * RETURNS:
 *    A pointer to the char directly after the new string ends (after the \0)
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
char *WS_URLDecodeInPlace(char *Value)
{
    char *Write;
    char *Read;
    char buff[100];

    Write=Value;
    Read=Value;
    while(*Read!=0)
    {
        if(*Read=='%')
        {
            /* Encoded */
            buff[0]=0;
            buff[1]=0;
            buff[2]=0;

            Read++; // Move past the %

            /* Copy the next 2 bytes */
            if(*Read!=0)
                buff[0]=*Read++;
            if(*Read!=0)
                buff[1]=*Read++;

            *Write++=strtol(buff,NULL,16);
        }
        else
        {
            *Write++=*Read;
            Read++;
        }
    }
    *Write++=0;
    return Write;
}

/*******************************************************************************
 * NAME:
 *    WS_URLEncode
 *
 * SYNOPSIS:
 *    bool WS_URLEncode(const char *Value,char *OutputBuffer,int MaxLen);
 *
 * PARAMETERS:
 *    Value [I] -- The string to encode.
 *    OutputBuffer [O] -- The buffer to store the encoded string in.  This
 *                        should be 3 times the size of Value for worst case.
 *    MaxLen [I] -- The size of 'OutputBuffer'.
 *
 * FUNCTION:
 *    This function encodes a string into URL encoding (%20 for space)
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- If we had to clip the string to keep it in 'MaxLen'.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
bool WS_URLEncode(const char *Value,char *OutputBuffer,int MaxLen)
{
    const char *Pos;
    char *Write;
    char *End;
    char c;

    Pos=Value;
    Write=OutputBuffer;
    End=OutputBuffer+MaxLen-1;
    while(*Pos!=0)
    {
        c=*Pos++;
        if((c>='A' && c<='Z') ||
            (c>='a' && c<='z') ||
            (c>='0' && c<='9') ||
            c=='-' || c=='_' || c=='.' || c=='~')
        {
            /* Normal copy */
            *Write++=c;
            if(Write==End)
            {
                *Write++=0;
                return false;
            }
        }
        else
        {
            /* % encode */
            if(Write+3>=End)
            {
                *Write++=0;
                return false;
            }
            sprintf(Write,"%%%02X",c);
            Write+=3;
        }
    }
    *Write=0;
    return true;
}

/*******************************************************************************
 * NAME:
 *    WS_Header
 *
 * SYNOPSIS:
 *    bool WS_Header(struct WebServer *Web,const char *Header);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Header [I] -- The header to write.
 *
 * FUNCTION:
 *    This function writes out a header as part of the http reply.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error (have you already started sending content?)
 *
 * NOTES:
 *    You do not need to end the string with a \r\n.  This function adds it
 *    for you.
 *
 * LIMITATIONS:
 *    You can not use this function if you have started sending content.
 *
 * SEE ALSO:
 *    WS_Start(), WS_WriteChunk()
 ******************************************************************************/
bool WS_Header(struct WebServer *Web,const char *Header)
{
    if(Header[0]==0)
        return false;

    if(Web->WriteStarted)
        return false;

    if(!Web->ReplyStarted)
        WS_StartReply(Web);

    SocketsCon_Write(&Web->Con,Header,strlen(Header));
    SocketsCon_Write(&Web->Con,"\r\n",2);

    return true;
}

/*******************************************************************************
 * NAME:
 *    WS_Location
 *
 * SYNOPSIS:
 *    bool WS_Location(struct WebServer *Web,const char *NewURL);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    NewURL [I] -- The new URL to redirect the browser to.
 *
 * FUNCTION:
 *    This function does a 301 Moved Permanently redirect.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error
 *
 * NOTES:
 *    This must be done before anything else is sent (content, header, etc)
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
bool WS_Location(struct WebServer *Web,const char *NewURL)
{
    if(NewURL[0]==0)
        return false;

    if(!WS_SetHTTPStatusCode(Web,e_ReplyStatus_MovedPerm))
        return false;

    SocketsCon_Write(&Web->Con,"Location: ",10);
    SocketsCon_Write(&Web->Con,NewURL,strlen(NewURL));
    SocketsCon_Write(&Web->Con,"\r\n",2);

    return true;
}

/*******************************************************************************
 * NAME:
 *    WS_SetHTTPStatusCode
 *
 * SYNOPSIS:
 *    bool WS_SetHTTPStatusCode(struct WebServer *Web,e_ReplyStatusType Code);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Code [I] -- The status code to set.  Only a limited set are supported.
 *
 * FUNCTION:
 *    This function sets the http reply status code that is sent out.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error
 *
 * NOTES:
 *    This must be done before anything else is sent (content, header, etc)
 *
 * SEE ALSO:
 *    WS_Location(), WS_Header()
 ******************************************************************************/
bool WS_SetHTTPStatusCode(struct WebServer *Web,e_ReplyStatusType Code)
{
    if(Code>=e_ReplyStatusMAX)
        return false;

    if(Web->WriteStarted)
        return false;

    if(Web->ReplyStarted)
        return false;

    Web->UserSetReplyStatus=true;
    Web->ReplyStatus=Code;
    WS_StartReply(Web);

    return true;
}

/*******************************************************************************
 * NAME:
 *    WS_GET
 *
 * SYNOPSIS:
 *    const char *WS_GET(struct WebServer *Web,const char *Arg);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Arg [I] -- The GET argument to search for an return
 *
 * FUNCTION:
 *    This function gets a GET arg from the request and returns it.
 *
 *    You must place all the GET args you are going to be using (or could want
 *    to use) in the Web->PageProp->Gets[] array for the system to be able
 *    read a GET arg.
 *
 * RETURNS:
 *    A pointer to the value or NULL if was not set.
 *
 * SEE ALSO:
 *    WS_Start(), WS_COOKIE(), WS_POST()
 ******************************************************************************/
const char *WS_GET(struct WebServer *Web,const char *Arg)
{
    /* GETS are first so no need to skip anything */
    return WS_FindArgInStorage(Web->ArgsStorage,Arg,Web->PageProp.Gets);
}

/*******************************************************************************
 * NAME:
 *    WS_COOKIE
 *
 * SYNOPSIS:
 *    const char *WS_COOKIE(struct WebServer *Web,const char *Arg);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Arg [I] -- The GET argument to search for an return
 *
 * FUNCTION:
 *    This function gets a COOKIE from the request and returns it.
 *
 *    You must place all the COOKIE names you are going to be using (or could
 *    want to use) in the Web->PageProp->Cookies[] array for the system to be
 *    able read a cookie.
 *
 * RETURNS:
 *    A pointer to the value or NULL if was not set.
 *
 * SEE ALSO:
 *    WS_Start(), WS_GET(), WS_POST()
 ******************************************************************************/
const char *WS_COOKIE(struct WebServer *Web,const char *Arg)
{
    char *Pos;

    Pos=Web->ArgsStorage;

    /* Skip all the GETS args */
    Pos=WS_SkipStorageArgs(Pos,Web->PageProp.Gets);
    return WS_FindArgInStorage(Pos,Arg,Web->PageProp.Cookies);
}

/*******************************************************************************
 * NAME:
 *    WS_POST
 *
 * SYNOPSIS:
 *    const char *WS_POST(struct WebServer *Web,const char *Arg);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Arg [I] -- The GET argument to search for an return
 *
 * FUNCTION:
 *    This function gets a POST arg from the request and returns it.
 *
 *    You must place all the POST args you are going to be using (or could
 *    want to use) in the Web->PageProp->Posts[] array for the system to be
 *    able read a POST arg.
 *
 * RETURNS:
 *    A pointer to the value or NULL if was not set.
 *
 * SEE ALSO:
 *    WS_Start(), WS_GET(), WS_COOKIE()
 ******************************************************************************/
const char *WS_POST(struct WebServer *Web,const char *Arg)
{
    char *Pos;

    Pos=Web->ArgsStorage;

    /* Skip all the GETS args */
    Pos=WS_SkipStorageArgs(Pos,Web->PageProp.Gets);
    /* Skip all the cookies */
    Pos=WS_SkipStorageArgs(Pos,Web->PageProp.Cookies);

    return WS_FindArgInStorage(Pos,Arg,Web->PageProp.Posts);
}

/*******************************************************************************
 * NAME:
 *    WS_FindArgInStorage
 *
 * SYNOPSIS:
 *    static const char *WS_FindArgInStorage(char *Pos,const char *Arg,
 *          const char **ArgsList)
 *
 * PARAMETERS:
 *    Pos [I] -- The starting pos in 'Web->ArgsStorage' to start searching
 *    Arg [I] -- The arg to search for
 *    ArgsList [I] -- The Web->PageProp->Gets[], Web->PageProp->Posts[], or
 *                    Web->PageProp->Cookies[] array to search.
 *
 * FUNCTION:
 *    This function searchs a section of the 'Web->ArgsStorage' looking for
 *    an arg.
 *
 *    You must set 'Pos' the start of the section you want to search.
 *
 * RETURNS:
 *    A pointer to the arg's value, or NULL if it was not found.
 *
 * SEE ALSO:
 *    WS_SkipStorageArgs()
 ******************************************************************************/
static const char *WS_FindArgInStorage(char *Pos,const char *Arg,
        const char **ArgsList)
{
    int g;

    if(ArgsList==NULL)
        return NULL;

    /* Find this arg */
    for(g=0;ArgsList[g]!=0;g++)
    {
        if(strcmp(ArgsList[g],Arg)==0)
        {
            /* Ok, this is the one */
            if(*Pos=='N')
                return NULL;
            return Pos+1;
        }

        /* Move to the next arg value */
        if(*Pos=='Y')
        {
            /* Skip the string (because the 'Y' is also a string we just skip
               to the end of the string instead of having to Pos++ before
               skipping) */
            while(*Pos++!=0)
                ;
        }
        else
        {
            /* Nothing after the Y/N so just skip the 'N' */
            Pos++;
        }
    }
    return NULL;
}

/*******************************************************************************
 * NAME:
 *    WS_SkipStorageArgs
 *
 * SYNOPSIS:
 *    static char *WS_SkipStorageArgs(char *StartingPos,const char **ArgsList);
 *
 * PARAMETERS:
 *    StartingPos [I] -- A pointer to the section to skip in the
 *                       'Web->ArgsStorage' array.
 *    ArgsList [I] -- The Web->PageProp->Gets[], Web->PageProp->Posts[], or
 *                    Web->PageProp->Cookies[] array to search.
 *
 * FUNCTION:
 *    This function skips a section in the 'Web->ArgsStorage' storage area.
 *
 *    Because the GETS, COOKIES, and POST are stored one after each other
 *    in the 'Web->ArgsStorage' array if you want to access the COOKIES you
 *    must skip the GETS.  This function provides this.
 *
 * RETURNS:
 *    A pointer to the next section.
 *
 * SEE ALSO:
 *    WS_FindArgInStorage()
 ******************************************************************************/
static char *WS_SkipStorageArgs(char *StartingPos,const char **ArgsList)
{
    char *Pos;
    int g;

    if(ArgsList==NULL)
        return StartingPos;

    /* Go over all the args */
    Pos=StartingPos;
    for(g=0;ArgsList[g]!=0;g++)
    {
        /* Move to the next arg value */
        if(*Pos=='Y')
        {
            /* Skip the string (because the 'Y' is also a string we just skip
               to the end of the string instead of having to Pos++ before
               skipping) */
            while(*Pos++!=0)
                ;
        }
        else
        {
            /* Nothing after the Y/N so just skip the 'N' */
            Pos++;
        }
    }

    return Pos;
}

/*******************************************************************************
 * NAME:
 *    WS_InsertCopy
 *
 * SYNOPSIS:
 *    static void WS_InsertCopy(char *Dest,char *DestEnd,const char *Src,
 *          int CopyLen);
 *
 * PARAMETERS:
 *    Dest [I] -- The point to insert the 'Src' string at
 *    DestEnd [I] -- The end of the data in the 'Web->ArgsStorage'
 *    Src [I] --  The string to insert.
 *    CopyLen [I] -- How many bytes to insert/copy
 *
 * FUNCTION:
 *    This function inserts a string into 'Web->ArgsStorage' by copying
 *    the content above the insert pos to past the end of the current data.
 *
 *    For example:
 *                       012345678901234
 *      Starting String: Cat In The Hat
 *      DestEnd=14;
 *      Dest=6;
 *      Src="/Out"
 *      CopyLen=4;
 *
 *      This will first copy as follows:
 *          "Cat In____ The Hat"
 *      and then copy the new string in:
 *          "Can In/Out The Hat"
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_InsertCopy(char *Dest,char *DestEnd,const char *Src,int CopyLen)
{
    char *p1;
    char *p2;

    /* Make room */
    p2=DestEnd+CopyLen-1;
    p1=p2-CopyLen;
    while(p1>=Dest)
        *p2--=*p1--;

    /* Insert the memory */
    memcpy(Dest,Src,CopyLen);
}

/*******************************************************************************
 * NAME:
 *    WS_SetCookie
 *
 * SYNOPSIS:
 *    bool WS_SetCookie(struct WebServer *Web,const char *Name,
 *          const char *Value,time_t Expire,const char *Path,const char *Domain,
 *          bool Secure,bool HttpOnly);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *    Name [I] -- Name of the cookie.  This can not have ' ', ',', or ';'.
 *    Value [I] -- The value of the cookie. This value is stored on the clients
 *                 computer; do not store sensitive information.  This is also
 *                 not escaped (or URI encoded).  You can not have the chars
 *                 ';', '=', '\r', '\n', ' ', or ','.  Short answer, URI encode
 *                 your string (WS_URLEncode).
 *    Expire [I] -- The time the cookie expires.  This is a time_t
 *                  (Unix timestamp) so is in number of seconds since the epoch.
 *                  This is sent into gmtime() to make a string.  Pass 0 to
 *                  ignore.
 *    Path [I] -- The path on the server in which the cookie will be available
 *                on. If set to '/', the cookie will be available within the
 *                entire domain. If set to '/foo/', the cookie will only be
 *                available within the /foo/ directory and all sub-directories
 *                such as /foo/bar/ of domain.  If you pass NULL the current
 *                directory that the cookie is being set in.
 *    Domain [I] -- The (sub)domain that the cookie is available to. Setting
 *                  this to a subdomain (such as 'www.example.com') will make
 *                  the cookie available to that subdomain and all other
 *                  sub-domains of it (i.e. w2.www.example.com). To make
 *                  the cookie available to the whole domain (including
 *                  all subdomains of it), simply set the value to the
 *                  domain name ('example.com', in this case).
 *
 *                  Older browsers still implementing the deprecated RFC 2109
 *                  may require a leading . to match all subdomains.
 *    Secure [I] -- Indicates that the cookie should only be transmitted over
 *                  a secure HTTPS connection from the client. When set to
 *                  true, the cookie will only be set if a secure connection
 *                  exists. On the server-side, it's on the programmer to
 *                  send this kind of cookie only on secure connection.
 *    HttpOnly [I] -- When true the cookie will be made accessible only
 *                    through the HTTP protocol. This means that the cookie
 *                    won't be accessible by scripting languages, such as
 *                    JavaScript. It has been suggested that this setting
 *                    can effectively help to reduce identity theft through
 *                    XSS attacks (although it is not supported by all
 *                    browsers), but that claim is often disputed.
 *
 * FUNCTION:
 *    This function sets a cookie to be sent to the client.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error (have you already started sending content?)
 *
 * NOTES:
 *    Most of this header was taken from the PHP doc's.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
bool WS_SetCookie(struct WebServer *Web,const char *Name,const char *Value,
        time_t Expire,const char *Path,const char *Domain,bool Secure,
        bool HttpOnly)
{
    struct tm *TheTm;
    char buff[100];
    static char mon_name[12][3] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun","Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static char wday_name[7][3] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char *Pos;

    if(Name[0]==0)
        return false;

    /* Check for bad char's */
    for(Pos=Name;*Pos!=0;Pos++)
        if(*Pos==' ' || *Pos==',' || *Pos==';' || *Pos=='\r' || *Pos=='\n')
            return false;

    for(Pos=Value;*Pos!=0;Pos++)
        if(*Pos==' ' || *Pos==',' || *Pos==';' || *Pos=='\r' || *Pos=='\n')
            return false;

    if(Web->WriteStarted)
        return false;

    if(!Web->ReplyStarted)
        WS_StartReply(Web);

    SocketsCon_Write(&Web->Con,"Set-Cookie: ",12);
    SocketsCon_Write(&Web->Con,Name,strlen(Name));
    SocketsCon_Write(&Web->Con,"=",1);
    SocketsCon_Write(&Web->Con,Value,strlen(Value));
    if(Expire!=0)
    {
        TheTm=gmtime(&Expire);
        sprintf(buff,"; Expires=%s, %02d %s %04d %02d:%02d:%02d GMT",
                wday_name[TheTm->tm_wday],
                TheTm->tm_mday,
                mon_name[TheTm->tm_mon],
                TheTm->tm_year+1900,
                TheTm->tm_hour,
                TheTm->tm_min,
                TheTm->tm_sec);
        SocketsCon_Write(&Web->Con,buff,strlen(buff));
    }
    if(Path!=NULL && Path[0]!=0)
    {
        SocketsCon_Write(&Web->Con,"; Path=",7);
        SocketsCon_Write(&Web->Con,Path,strlen(Path));
    }
    if(Domain!=NULL && Domain[0]!=0)
    {
        SocketsCon_Write(&Web->Con,"; Domain=",7);
        SocketsCon_Write(&Web->Con,Domain,strlen(Domain));
    }
    if(Secure)
    {
        SocketsCon_Write(&Web->Con,"; Secure",8);
    }
    if(HttpOnly)
    {
        SocketsCon_Write(&Web->Con,"; HttpOnly",10);
    }

    SocketsCon_Write(&Web->Con,"\r\n",2);

    return true;
}

/*******************************************************************************
 * NAME:
 *    WS_GetOSSocketHandles
 *
 * SYNOPSIS:
 *    int WS_GetOSSocketHandles(t_ConSocketHandle *Handles);
 *
 * PARAMETERS:
 *    Handles [O] -- An array to fill in with the handles being used by the
 *                   web server.  This must be at least
 *                   'WS_OPT_MAX_CONNECTIONS+1' in size.
 *
 * FUNCTION:
 *    This function gets all the socket handles being used by the web server.
 *    You can use these handles to wait on.
 *
 *    The returned handles depend on what 't_ConSocketHandle' is defined as.
 *
 * RETURNS:
 *    The number of handles being returned
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
int WS_GetOSSocketHandles(t_ConSocketHandle *Handles)
{
    t_ConSocketHandle SocketHandle;
    int r;
    int InsertPos;

    /* Fill in the first entry with the listening socket */
    InsertPos=0;
    SocketsCon_GetSocketHandle(&m_ListeningSocket,&Handles[InsertPos++]);
    for(r=0;r<WS_OPT_MAX_CONNECTIONS;r++)
        if(SocketsCon_GetSocketHandle(&m_WebServers[r].Con,&SocketHandle))
            Handles[InsertPos++]=SocketHandle;

    return InsertPos;
}

/*******************************************************************************
 * NAME:
 *    WS_ProcessGetVars
 *
 * SYNOPSIS:
 *    static void WS_ProcessGetVars(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function goes though the URI line (in 'Web->LineBuff') and pulls
 *    out the GET args and stores them in 'Web->ArgsStorage'.
 *
 * RETURNS:
 *    NONE
 *
 * NOTES:
 *    The format of 'ArgsStorage' is that we store each variable
 *    (GET/POST/COOKIE) in the order they are in listed in the array of
 *    names.  We store a Y or an N depending on if we found the var.  If
 *    it's an N then the next var is right after it in the next byte (0 bytes
 *    between).  If it is Y then we store the string followed by a \0.
 *
 *    We store the GETs first, then the COOKIEs, then POSTs.
 *
 *    For example:
 *      struct WSPageProp Props=
 *      {
 *          .Gets={"one","two","three",NULL},
 *          .Cookies={"four","five",NULL},
 *          .Posts={"six","seven",NULL},
 *      }
 *
 *    Where nothing is passed in then 'ArgsStorage' will look like:
 *      NNNNNNN
 *
 *    Where one is passed in (?one=value1):
 *      Yvalue1\0NNNNNN
 *
 *    Where two is passed in (?two=value2):
 *      NYvalue2\0NNNNN
 *
 *    Where one and two are passed in (?two=value2&one=value1):
 *      Yvalue1\0Yvalue2\0NNNNN
 *
 *    Where the 'five' cookie is set:
 *      NNNNYcookie\0NN
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_ProcessGetVars(struct WebServer *Web)
{
    char *ArgsStart;
    char *Start;
    char *Pos;
    char *Write;
    char *StorageStart;
    char *StartOfLastWrite;
    int g;
    int ArgCount;
    int arg;
    int Len;
    bool Found;

    /* Find the end of the string (it will be the start of the args or the real
       end of the string) */
    ArgsStart=Web->LineBuff;
    while(*ArgsStart!=0)
        ArgsStart++;
    ArgsStart++;    // Move to the start of args

    ArgCount=0;
    Pos=ArgsStart;
    /* We need to break up the args first */
    while(*Pos!=0)
    {
        if(*Pos=='&')
        {
            *Pos=0;
            ArgCount++;
        }
        Pos++;
    }
    if(Pos!=ArgsStart)
        ArgCount++;

    StorageStart=Web->ArgsStorage;
    Write=StorageStart;
    StartOfLastWrite=Write;

    /* See if this arg is in the list of get args */
    if(Web->PageProp.Gets!=NULL)
    {
        for(g=0;Web->PageProp.Gets[g]!=0;g++)
        {
            /* See if this arg is in the args we had sent in */
            arg=0;
            Pos=ArgsStart;
            Start=Pos;
            while(arg<ArgCount)
            {
                if(*Pos=='=')
                {
                    /* At the value */
                    Len=strlen(Web->PageProp.Gets[g]);
                    if(Len==Pos-Start &&
                            strncmp(Start,Web->PageProp.Gets[g],Len)==0)
                    {
                        /* Found this arg */
                        Pos++;  // Move past the '='
                        break;
                    }
                }
                if(*Pos==0)
                {
                    /* End of the arg, check it */
                    arg++;
                    Start=Pos+1;
                }
                Pos++;
            }
            Found=false;
            if(arg<ArgCount)
                Found=true;

            /* Store if we found it */
            if((Write-StorageStart)>=WS_OPT_ARG_MEMORY_SIZE)
            {
                Web->ReplyStatus=e_ReplyStatus_InsufficientStorage;
                return;
            }
            *Write++=Found?'Y':'N';

            if(Found)
            {
                /* See if we have space for the value */
                while(*Pos!=0)
                {
                    if((Write-StorageStart)>=WS_OPT_ARG_MEMORY_SIZE)
                    {
                        Web->ReplyStatus=e_ReplyStatus_InsufficientStorage;
                        return;
                    }
                    *Write++=*Pos++;
                }
                if((Write-StorageStart)>=WS_OPT_ARG_MEMORY_SIZE)
                {
                    Web->ReplyStatus=e_ReplyStatus_InsufficientStorage;
                    return;
                }
                *Write++=0;

                /* Decode in place, and then use the end of the string as
                   where to continue writing new args (if the string shrinks
                   use the new end) */
                Write=WS_URLDecodeInPlace(StartOfLastWrite);
            }
            StartOfLastWrite=Write;
        }
    }

    /* Fill in the cookies and post vars with all not found (so the
       cookie/post code doesn't have to handle the unknown state) */
    if(Web->PageProp.Cookies!=NULL)
    {
        for(g=0;Web->PageProp.Cookies[g]!=0;g++)
        {
            if((Write-StorageStart)>=WS_OPT_ARG_MEMORY_SIZE)
            {
                Web->ReplyStatus=e_ReplyStatus_InsufficientStorage;
                return;
            }
            *Write++='N';
        }
    }

    if(Web->PageProp.Posts!=NULL)
    {
        for(g=0;Web->PageProp.Posts[g]!=0;g++)
        {
            if((Write-StorageStart)>=WS_OPT_ARG_MEMORY_SIZE)
            {
                Web->ReplyStatus=e_ReplyStatus_InsufficientStorage;
                return;
            }
            *Write++='N';
        }
    }
//DEBUG_PrintStoredArgs(Web);
}

/*******************************************************************************
 * NAME:
 *    WS_ProcessCookieVars
 *
 * SYNOPSIS:
 *    static void WS_ProcessCookieVars(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function processes a "Cookie: " header.  It will set the cookies
 *    value in 'Web->ArgsStorage' if any of the sent in cookies match the
 *    list of supported cookies.
 *
 *    See WS_ProcessGetVars() for a description of how 'ArgsStorage' works.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_ProcessCookieVars(struct WebServer *Web)
{
    int g;
    char *ArgsStart;
    int ArgCount;
    char *Pos;
    char *StorageStart;
    char *EndOfStorage;
    char *Write;
    int arg;
    char *Start;
    int Len;

    /* Break the header line up into each cookie */
    /* Find the end of the Cookie: string */
    ArgsStart=Web->LineBuff;
    while(*ArgsStart!=':' && *ArgsStart!=0)
        ArgsStart++;

    /* Skip the : */
    if(*ArgsStart==':')
        ArgsStart++;

    /* Skip the space (There should be a space, but we handle if there isn't) */
    if(*ArgsStart==' ')
        ArgsStart++;

    /* See if there are any cookies */
    if(*ArgsStart==0)
        return;

    ArgCount=0;
    Pos=ArgsStart;
    /* We need to break up the args first */
    while(*Pos!=0)
    {
        if(*Pos==';')
        {
            *Pos=0;
            ArgCount++;
        }
        Pos++;
    }
    if(Pos!=ArgsStart)
        ArgCount++;

    /* Find the end of the GET vars (where the cookies start) */
    StorageStart=Web->ArgsStorage;
    Write=StorageStart;

    if(Web->PageProp.Gets!=NULL)
    {
        /* Skip all the GETS args */
        Write=WS_SkipStorageArgs(Write,Web->PageProp.Gets);
    }

    /* Find the end of the 'ArgsStorage' storage */
    EndOfStorage=Write; // Already skipped the GETS
    if(Web->PageProp.Cookies!=NULL)
    {
        /* Skip all the COOKIES args */
        EndOfStorage=WS_SkipStorageArgs(EndOfStorage,Web->PageProp.Cookies);
    }
    if(Web->PageProp.Posts!=NULL)
    {
        /* Skip all the POSTS args */
        EndOfStorage=WS_SkipStorageArgs(EndOfStorage,Web->PageProp.Posts);
    }

    /* See if this arg is in the list of cookie args */
    if(Web->PageProp.Cookies!=NULL)
    {
        for(g=0;Web->PageProp.Cookies[g]!=0;g++)
        {
            /* See if this arg is in the args we had sent in */
            arg=0;
            Pos=ArgsStart;
            Start=Pos;
            /* Skip any spaces at the start */
            while(*Start==' ')
                Start++;
            while(arg<ArgCount)
            {
                if(*Pos=='=')
                {
                    /* At the value */
                    Len=strlen(Web->PageProp.Cookies[g]);
                    /* Skip any spaces at the start of the name */
                    while(*Start==' ')
                        Start++;
                    if(Len==Pos-Start &&
                            strncmp(Start,Web->PageProp.Cookies[g],Len)==0)
                    {
                        /* Found this arg */
                        Pos++;  // Move past the '='
                        break;
                    }
                }
                if(*Pos==0)
                {
                    /* End of the arg */
                    arg++;
                    Start=Pos+1;
                }
                Pos++;
            }
            /* Did we find it? */
            if(arg<ArgCount)
            {
                /* Yep */
                Len=strlen(Pos);

                /* See if we have already seen this one */
                if(*Write=='Y')
                {
                    /* Yep, skip */
                    while(*Write++!=0)
                        ;
                }
                else
                {
                    /* Make sure we have space */
                    if((Write-StorageStart)+Len>=WS_OPT_ARG_MEMORY_SIZE)
                    {
                        Web->ReplyStatus=e_ReplyStatus_InsufficientStorage;
                        return;
                    }
                    WS_InsertCopy(Write+1,EndOfStorage,Pos,Len+1);  // +1 for the end of the string
                    *Write++='Y';
                    Write+=Len+1;   // +1 for the end of the string
                    EndOfStorage+=Len+1;    // +1 for the end of the string
                }
            }
            else
            {
                /* Skip the 'N' we already have */
                Write++;
            }
        }
    }

//DEBUG_PrintStoredArgs(Web);
}

/*******************************************************************************
 * NAME:
 *    WS_StartProcessingPOSTVar
 *
 * SYNOPSIS:
 *    static void WS_StartProcessingPOSTVar(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function sets up for processing of a POST var.  It will find the
 *    var in the list of POST vars and setup Web->PostWritePos to the
 *    pos where the POST value should be written, and Web->PostEndOfStorage
 *    to the end of arg storage.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void WS_StartProcessingPOSTVar(struct WebServer *Web)
{
    char *Write;
    char *StorageStart;
    int p;
    unsigned int Len;

    StorageStart=Web->ArgsStorage;
    Write=StorageStart;
    Web->PostWritePos=NULL;

    if(Web->PageProp.Gets!=NULL)
    {
        /* Skip all the GETS args */
        Write=WS_SkipStorageArgs(Write,Web->PageProp.Gets);
    }
    if(Web->PageProp.Cookies!=NULL)
    {
        /* Skip all the COOKIE args */
        Write=WS_SkipStorageArgs(Write,Web->PageProp.Cookies);
    }

    /* See if this arg is in the list of get args */
    if(Web->PageProp.Posts!=NULL)
    {
        /* Skip all the POST args (this will be the end of storage) */
        Web->PostEndOfStorage=WS_SkipStorageArgs(Write,Web->PageProp.Posts);

        for(p=0;Web->PageProp.Posts[p]!=0;p++)
        {
            /* See if this arg is in the args we had sent in */
            Len=strlen(Web->LineBuff);
            if(strncmp(Web->LineBuff,Web->PageProp.Posts[p],Len)==0 &&
                    Web->PageProp.Posts[p][Len]==0)
            {
                /* Found this arg */
                *Write++='Y';

                Web->PostWritePos=Write;
                break;
            }
            else
            {
                /* Skip this arg */
                if(*Write=='Y')
                {
                    while(*Write!=0)
                        Write++;
                }
                Write++;    // Skip the N or the \0
            }
        }
    }
}

/*******************************************************************************
 * NAME:
 *    WS_CopyLineBuffer2POSTVar
 *
 * SYNOPSIS:
 *    static bool WS_CopyLineBuffer2POSTVar(struct WebServer *Web);
 *
 * PARAMETERS:
 *    Web [I] -- The web server context to work on
 *
 * FUNCTION:
 *    This function copies what is in the line buffer to the POST var storage.
 *
 *    You have to have started this with WS_StartProcessingPOSTVar() to setup
 *    the needed vars.
 *
 * RETURNS:
 *    true -- All ok
 *    false -- There wasn't enough room to store this var.
 *
 * SEE ALSO:
 *    WS_StartProcessingPOSTVar()
 ******************************************************************************/
static bool WS_CopyLineBuffer2POSTVar(struct WebServer *Web)
{
    char EscBuff[3];
    char *Pos;
    char *EndOfLineBuff;
    int Len;
    char Zero;

    /* Make the line buffer in to a string */
    Web->LineBuff[Web->LineBuffPos]=0;

//{
//printf("\33[1;32m");
//Pos=Web->LineBuff;
//while(*Pos!=0)
//    printf("%c",*Pos++);
//printf("\33[0m\r\n");
//}
    if(Web->PostWritePos!=NULL)
    {
        /* First see we where in the middle of a % esc seq */
        EscBuff[0]=0;
        EscBuff[1]=0;
        EscBuff[2]=0;
        if(Web->LineBuffPos>=2)
        {
            /* Ok, move the esc seq to 'EscBuff' and kill it out of the main
               buffer */
            Pos=&Web->LineBuff[Web->LineBuffPos-1];
            if(*Pos=='%')
            {
                EscBuff[0]='%';
                *Pos=0;
                Web->LineBuffPos--;
            }
            else
            {
                Pos--;
                if(*Pos=='%')
                {
                    EscBuff[0]='%';
                    EscBuff[1]=*(Pos+1);
                    *Pos=0;
                    Web->LineBuffPos-=2;
                }
            }
        }

        /* Decode the line buffer (we have to handle the + thing before we
           decode it) */
        Pos=Web->LineBuff;
        while(*Pos!=0)
        {
            if(*Pos=='+')
                *Pos=' ';
            Pos++;
        }
        EndOfLineBuff=WS_URLDecodeInPlace(Web->LineBuff);

        Len=EndOfLineBuff-Web->LineBuff;

        /* Make sure we can fit this */
        if(Web->PostEndOfStorage+Len>Web->ArgsStorage+WS_OPT_ARG_MEMORY_SIZE)
        {
            /* It's not going to fit, clip it and return insufficient storage */
            Zero=0;
            WS_InsertCopy(Web->PostWritePos,Web->PostEndOfStorage,&Zero,1);
            Web->ReplyStatus=e_ReplyStatus_InsufficientStorage;
            return false;
        }

        WS_InsertCopy(Web->PostWritePos,Web->PostEndOfStorage,Web->LineBuff,
                Len);
        Web->PostEndOfStorage+=Len;

        /* Move to the end of what we just inserted */
        Web->PostWritePos+=Len-1;

        /* Ok, if we where in the middle of an esc seq, but it in the buffer */
        strcpy(Web->LineBuff,EscBuff);
        Web->LineBuffPos=strlen(EscBuff);
    }
    else
    {
        Web->LineBuffPos=0;
    }
//DEBUG_PrintStoredArgs(Web);
    return true;
}

//static void DEBUG_PrintStoredArgs(struct WebServer *Web)
//{
//    char *StorageStart;
//    int r;
//    int rr;
//    char c;
//    char *Write;
//    int g;
//
//    StorageStart=Web->ArgsStorage;
//
//    printf("\r\nARGS DUMP:\r\n");
//    for(r=0;r<16*4;r++)
//    {
//        printf("%02X ",(unsigned char)StorageStart[r]);
//        if((r&15)==15)
//        {
//            for(rr=0;rr<16;rr++)
//            {
//                c=StorageStart[r-15+rr];
//                printf("%c",c<32||c>127?'.':c);
//            }
//            printf("\r\n");
//        }
//    }
//
//    Write=StorageStart;
//    if(Web->PageProp.Gets!=NULL)
//    {
//        for(g=0;Web->PageProp.Gets[g]!=0;g++)
//        {
//            if(*Write=='Y')
//            {
//                printf("GET %s=\"%s\"\r\n",Web->PageProp.Gets[g],Write+1);
//                while(*Write!=0)
//                    Write++;
//            }
//            else
//            {
//                printf("GET %s not found\r\n",Web->PageProp.Gets[g]);
//            }
//            Write++;
//        }
//    }
//
//    if(Web->PageProp.Cookies!=NULL)
//    {
//        for(g=0;Web->PageProp.Cookies[g]!=0;g++)
//        {
//            if(*Write=='Y')
//            {
//                printf("COOKIE %s=\"%s\"\r\n",Web->PageProp.Cookies[g],Write+1);
//                while(*Write!=0)
//                    Write++;
//            }
//            else
//            {
//                printf("COOKIE %s not found\r\n",Web->PageProp.Cookies[g]);
//            }
//            Write++;
//        }
//    }
//
//    if(Web->PageProp.Posts!=NULL)
//    {
//        for(g=0;Web->PageProp.Posts[g]!=0;g++)
//        {
//            if(*Write=='Y')
//            {
//                printf("POST %s=\"%s\"\r\n",Web->PageProp.Posts[g],Write+1);
//                while(*Write!=0)
//                    Write++;
//            }
//            else
//            {
//                printf("POST %s not found\r\n",Web->PageProp.Posts[g]);
//            }
//            Write++;
//        }
//    }
//}

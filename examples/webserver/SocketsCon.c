/*******************************************************************************
 * FILENAME: SocketsCon.c
 *
 * PROJECT:
 *    Bitty HTTP
 *
 * FILE DESCRIPTION:
 *    This version is a basic version of the sockets connection system.  It
 *    takes out all of the SSL stuff so we don't have to link in a much stuff.
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
#include "SocketsCon.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <signal.h>

/*** DEFINES                  ***/
#define CONNECT_TIMEOUT 10000   // How long do we wait before giving up on a connect() (ms)

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/

/*** FUNCTION PROTOTYPES      ***/
static void PRIV_SocketsCon_Error(struct SocketCon *Con,
        e_ConnectErrorType ErrorCode);
static uint32_t SocketsCon_Get1mSecCounter(void);

/*** VARIABLE DEFINITIONS     ***/

/*******************************************************************************
 * NAME:
 *    SocketsCon_Get1mSecCounter
 *
 * SYNOPSIS:
 *    static uint32_t SocketsCon_Get1mSecCounter(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    Helper function that gets the number of milliseconds (used for ms timers)
 *
 * RETURNS:
 *    The number of milliseconds ticks (ok, it's the time, but we pretend it's
 *    the number of milliseconds since we started)
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static uint32_t SocketsCon_Get1mSecCounter(void)
{
    struct timeval te; 
    gettimeofday(&te, NULL); /* get current time */
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; /* caculate milliseconds */
    return (uint32_t)milliseconds;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_InitSocketConSystem
 *
 * SYNOPSIS:
 *    bool SocketsCon_InitSocketConSystem(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    This function init's my socket connection wrapper system.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
bool SocketsCon_InitSocketConSystem(void)
{
    /* Disable the the signal on send() to a closed connection (return an
       error code instead) */
    signal(SIGPIPE, SIG_IGN);

    return true;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_ShutdownSocketConSystem
 *
 * SYNOPSIS:
 *    void SocketsCon_ShutdownSocketConSystem(void);
 *
 * PARAMETERS:
 *    NONE
 *
 * FUNCTION:
 *    This function shuts down the socket connection wrapper system.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void SocketsCon_ShutdownSocketConSystem(void)
{
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_InitSockCon
 *
 * SYNOPSIS:
 *    bool SocketsCon_InitSockCon(struct SocketCon *Con);
 *
 * PARAMETERS:
 *    Con [O] -- The connection to init
 *
 * FUNCTION:
 *    This function init's a connection structure.  This does not actually
 *    open or allocate any resources just sets up the structure.
 *
 * RETURNS:
 *    true -- Everything was fine
 *    false -- There was an error.  This means that one of the args is invalid.
 *
 * SEE ALSO:
 *    SocketsCon_Connect()
 ******************************************************************************/
bool SocketsCon_InitSockCon(struct SocketCon *Con)
{
    Con->ErrorCode=e_ConnectError_AllOk;
    Con->State=e_ConnectState_Idle;
    Con->SocketFD=-1;
    Con->ReadInProgress=false;

    return true;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_Connect
 *
 * SYNOPSIS:
 *    bool SocketsCon_Connect(struct SocketCon *Con,const char *ServerName,
 *          int portNo);
 *
 * PARAMETERS:
 *    Con [I/O] -- The connection to work on
 *    ServerName [I] -- The server name or IP address
 *    portNo [I] -- What port to connect to
 *
 * FUNCTION:
 *    This function starts a nonblocking connection to a server.
 *
 * RETURNS:
 *    true -- Things have been started
 *    false -- There was an error
 *
 * SEE ALSO:
 *    SocketsCon_InitSockCon(), SocketsCon_Write(), SocketsCon_Read(),
 *    SocketsCon_Close()
 ******************************************************************************/
bool SocketsCon_Connect(struct SocketCon *Con,const char *ServerName,
        int portNo)
{
    struct hostent *host;
    struct sockaddr_in serv_addr;
    int flags;
    int ConnectRet;

    Con->SocketFD=0;

    host=gethostbyname(ServerName);
    if(host==NULL)
    {
        PRIV_SocketsCon_Error(Con,e_ConnectError_gethostbynameFailed);
        return false;
    }

    if((Con->SocketFD=socket(AF_INET,SOCK_STREAM,0))<0)
    {
        PRIV_SocketsCon_Error(Con,e_ConnectError_Failed2GetSocket);
        return false;
    }

    /* Switch to nonblocking */
    flags=fcntl(Con->SocketFD, F_GETFL, 0);
    if(flags<0)
        flags=0;
    flags|=O_NONBLOCK;
    fcntl(Con->SocketFD, F_SETFL, flags);

    memset(&serv_addr,'0',sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portNo); 
    serv_addr.sin_addr.s_addr = *(long*)(host->h_addr);
    memset(&(serv_addr.sin_zero), 0, 8);

    ConnectRet=connect(Con->SocketFD,(struct sockaddr *)&serv_addr,
            sizeof(serv_addr));
    Con->Last_errno=errno;
    if(ConnectRet<0 && errno!=EINPROGRESS)
    {
        /* We had an error */
        PRIV_SocketsCon_Error(Con,e_ConnectError_ConnectFailed);
        return false;
    }

    Con->State=e_ConnectState_Connecting;
    Con->TimeoutTS=SocketsCon_Get1mSecCounter();

    return true;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_EnableAddressReuse
 *
 * SYNOPSIS:
 *    bool SocketsCon_EnableAddressReuse(struct SocketCon *Con,bool Enable);
 *
 * PARAMETERS:
 *    Con [I/O] -- The connection to work on
 *    Enable [I] -- If true turns on address reuse, false turns it off.
 *
 * FUNCTION:
 *    This function disables the WAIT_TIME for a socket by turning on
 *    SO_REUSEADDR.
 *
 *    This will make it so you don't have to wait the WAIT_TIME before reopening
 *    a socket on a port (you get a ERROR on binding:Address already in use
 *    when trying to open the socket).
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error and it could not be set.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
bool SocketsCon_EnableAddressReuse(struct SocketCon *Con,bool Enable)
{
    int newsetting;

    if(Enable)
        newsetting=1;
    else
        newsetting=0;

    if(setsockopt(Con->SocketFD,SOL_SOCKET,SO_REUSEADDR,&newsetting,
            sizeof(int))<0)
    {
        /* We had an error */
        return false;
    }
    return true;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_Tick
 *
 * SYNOPSIS:
 *    void SocketsCon_Tick(struct SocketCon *Con);
 *
 * PARAMETERS:
 *    Con [I/O] -- The connection to work on
 *
 * FUNCTION:
 *    This function keeps the connection happy.  It manages connecting to a
 *    server and any other house keeping that is needed.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void SocketsCon_Tick(struct SocketCon *Con)
{
    fd_set rset, wset;
    struct timeval tval;
    int error;
    socklen_t len;
    int flags;

    switch(Con->State)
    {
        case e_ConnectState_Idle:
        break;
        case e_ConnectState_Connecting:
            FD_ZERO(&rset);
            FD_SET(Con->SocketFD, &rset);
            wset = rset;
            tval.tv_sec = 0;
            tval.tv_usec = 0;
            if(select(Con->SocketFD+1,&rset,&wset,NULL,&tval)==0)
            {
                /* Continue waiting */
                if(SocketsCon_Get1mSecCounter()-Con->TimeoutTS>CONNECT_TIMEOUT)
                {
                    /* We are giving up */
                    PRIV_SocketsCon_Error(Con,e_ConnectError_Failed2Connect);
                }

                return;
            }

            if(FD_ISSET(Con->SocketFD,&rset) || FD_ISSET(Con->SocketFD,&wset))
            {
                len=sizeof(error);
                if(getsockopt(Con->SocketFD,SOL_SOCKET,SO_ERROR,&error,&len)<0)
                {
                    PRIV_SocketsCon_Error(Con,e_ConnectError_Failed2Getsockopt);
                    return;
                }
                if(error)
                {
                    PRIV_SocketsCon_Error(Con,e_ConnectError_Failed2Getsockopt);
                    return;
                }
                /* Connection made */

                /* Change to nonblocking */
                flags=fcntl(Con->SocketFD,F_GETFL,0);
                if(flags<0)
                {
                    PRIV_SocketsCon_Error(Con,
                            e_ConnectError_Failed2Change2NonBlocking);
                    return;
                }
                flags|=O_NONBLOCK;
                if(fcntl(Con->SocketFD,F_SETFL,flags)!=0)
                {
                    PRIV_SocketsCon_Error(Con,
                            e_ConnectError_Failed2Change2NonBlocking);
                    return;
                }

                Con->State=e_ConnectState_Connected;
            }
            else
            {
                PRIV_SocketsCon_Error(Con,e_ConnectError_SelectFailed);
                return;
            }
        break;
        case e_ConnectState_Connected:
        break;
        case e_ConnectState_Error:
        case e_ConnectState_Listening:
        break;
        case e_ConnectStateMAX:
        break;
    }
}

/*******************************************************************************
 * NAME:
 *    PRIV_SocketsCon_Error
 *
 * SYNOPSIS:
 *    static void PRIV_SocketsCon_Error(struct SocketCon *Con,
 *              e_ConnectErrorType ErrorCode);
 *
 * PARAMETERS:
 *    Con [I/O] -- The connection to work on
 *    ErrorCode [I] -- The error code we had
 *
 * FUNCTION:
 *    This function is a private function that sets a connection to the error
 *    state and frees all it's resourses.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
static void PRIV_SocketsCon_Error(struct SocketCon *Con,
        e_ConnectErrorType ErrorCode)
{
    Con->State=e_ConnectState_Error;

    if(Con->SocketFD>=0)
        close(Con->SocketFD);

    Con->ErrorCode=ErrorCode;

    Con->SocketFD=-1;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_Write
 *
 * SYNOPSIS:
 *    bool SocketsCon_Write(struct SocketCon *Con,const void *buf,int num);
 *
 * PARAMETERS:
 *    Con [I/O] -- The connection to work on
 *    buf [I] -- The buffer to send
 *    num [I] -- The number of bytes to send
 *
 * FUNCTION:
 *    This function sends data out a socket.
 *
 * RETURNS:
 *    true -- Things worked out
 *    false -- There was an error
 *
 * SEE ALSO:
 *    SocketsCon_Connect(), SocketsCon_Read()
 ******************************************************************************/
bool SocketsCon_Write(struct SocketCon *Con,const void *buf,int num)
{
    int retVal;
    int BytesSent;
    const uint8_t *OutputPos;

    if(Con->State!=e_ConnectState_Connected)
        return false;

    BytesSent=0;
    OutputPos=buf;
    while(BytesSent<num)
    {
        retVal=write(Con->SocketFD,OutputPos,num-BytesSent);
        Con->Last_errno=errno;
        if(retVal<0)
        {
            /* If it was busy, try again */
            if(Con->Last_errno==EAGAIN)
                continue;

            /* Real error */
            PRIV_SocketsCon_Error(Con,e_ConnectError_WriteTX_SOCKET_ERROR);
            return false;
        }
        BytesSent+=retVal;
        OutputPos+=retVal;
    }
    return true;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_Read
 *
 * SYNOPSIS:
 *    int SocketsCon_Read(struct SocketCon *Con,void *buf,int num);
 *
 * PARAMETERS:
 *    Con [I/O] -- The connection to work on
 *    buf [I] -- The buffer to read into
 *    num [I] -- The max number of bytes that can be read into 'buf'
 *
 * FUNCTION:
 *    This function reads data from a connection.  It will return immediately
 *    if there is no data.
 *
 * RETURNS:
 *    The number of bytes read from the connection or <0 if there was an error.
 *
 * SEE ALSO:
 *    SocketsCon_Connect(), SocketsCon_Write()
 ******************************************************************************/
int SocketsCon_Read(struct SocketCon *Con,void *buf,int num)
{
    struct timeval tv;
    fd_set fds;
    int retVal;

    if(Con->State==e_ConnectState_Error)
        return -100;

    if(Con->State!=e_ConnectState_Connected)
        return 0;

    retVal=0;

    FD_ZERO(&fds);
    FD_SET(Con->SocketFD,&fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    if(Con->ReadInProgress || select(Con->SocketFD+1,&fds,NULL,NULL,&tv)!=0)
    {
        if(Con->ReadInProgress || FD_ISSET(Con->SocketFD,&fds))
        {
            Con->ReadInProgress=false;

            retVal=read(Con->SocketFD,buf,num);
            Con->Last_errno=errno;
            if(retVal==0)
            {
                /* 0=connection closed (because we where already told
                   there was data) */
                Con->State=e_ConnectState_Idle;
                retVal=-55;
            }
        }
    }
    return retVal;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_Close
 *
 * SYNOPSIS:
 *    void SocketsCon_Close(struct SocketCon *Con);
 *
 * PARAMETERS:
 *    Con [I/O] -- The connection to work on
 *
 * FUNCTION:
 *    This function closes down a connection.  It frees all the resourses.
 *
 * RETURNS:
 *    NONE
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
void SocketsCon_Close(struct SocketCon *Con)
{
    Con->State=e_ConnectState_Idle;

    if(Con->SocketFD>=0)
        close(Con->SocketFD);

    Con->SocketFD=-1;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_Listen
 *
 * SYNOPSIS:
 *    bool SocketsCon_Listen(struct SocketCon *Con,const char *bindadd,
 *          int PortNo);
 *
 * PARAMETERS:
 *    Con [I/O] -- The connection to work on
 *    bindadd [I] -- The address to bind to (pass NULL for INADDR_ANY)
 *    PortNo [I] -- The port number to bind to
 *
 * FUNCTION:
 *    This function opens a socket, binds to it, and starts listening.
 *
 * RETURNS:
 *    true -- things worked out
 *    false -- There was an error
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
bool SocketsCon_Listen(struct SocketCon *Con,const char *bindadd,int PortNo)
{
    struct sockaddr_in serv_addr;

    Con->SocketFD=socket(AF_INET,SOCK_STREAM,0);
    if(Con->SocketFD<0)
    {
        PRIV_SocketsCon_Error(Con,e_ConnectError_Failed2GetSocket);
        return false;
    }

    /* For DEBUG we disable having to wait for the socket to finally close
       (so we can stop and restart without waiting a few minutes) */
    {
        int enable = 1;

        /* We ignore errors from this */
        setsockopt(Con->SocketFD,SOL_SOCKET,SO_REUSEADDR,&enable,sizeof(int));
    }

    /* Initialize socket structure */
    bzero((char *)&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    if(bindadd==NULL)
        serv_addr.sin_addr.s_addr=INADDR_ANY;
    else
        serv_addr.sin_addr.s_addr=inet_addr(bindadd);
    serv_addr.sin_port=htons(PortNo);

    if(bind(Con->SocketFD,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
    {
        PRIV_SocketsCon_Error(Con,e_ConnectError_Failed2Bind);
        return false;
    }

    listen(Con->SocketFD,5);

    Con->State=e_ConnectState_Listening;

    return true;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_Accept
 *
 * SYNOPSIS:
 *    bool SocketsCon_Accept(struct SocketCon *Con,
 *              struct SocketCon *NewCon);
 *
 * PARAMETERS:
 *    Con [I/O] -- The connection to work on
 *    NewCon [O] -- The new accepted connection.  This must have
 *                  SocketsCon_InitSockCon() called on it before calling this
 *                  function.
 *
 * FUNCTION:
 *    This function accepts a new connection on a listening socket.
 *
 * RETURNS:
 *    true -- We got a new connection
 *    false -- No new connection or error
 *
 * SEE ALSO:
 *    SocketsCon_Listen()
 ******************************************************************************/
bool SocketsCon_Accept(struct SocketCon *Con,struct SocketCon *NewCon)
{
    struct timeval tv;
    fd_set fds;
    int newsockfd;
    socklen_t clilen;
    struct sockaddr_in cli_addr;
    int flags;

    clilen=sizeof(cli_addr);

    FD_ZERO(&fds);
    FD_SET(Con->SocketFD,&fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if(select(Con->SocketFD+1,&fds,NULL,NULL,&tv)!=0)
    {
        if(FD_ISSET(Con->SocketFD,&fds))
        {
            /* We have a new connection coming in */
            newsockfd=accept(Con->SocketFD,(struct sockaddr *)&cli_addr,
                    &clilen);
            if(newsockfd<0)
            {
                PRIV_SocketsCon_Error(Con,e_ConnectError_AcceptError);
                return false;
            }
            NewCon->SocketFD=newsockfd;

            /* Switch to nonblocking */
            flags=fcntl(NewCon->SocketFD, F_GETFL, 0);
            if(flags<0)
                flags=0;
            flags|=O_NONBLOCK;
            fcntl(NewCon->SocketFD, F_SETFL, flags);

            NewCon->State=e_ConnectState_Connected;
            return true;
        }
    }
    return false;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_HasError
 *
 * SYNOPSIS:
 *    bool SocketsCon_HasError(struct SocketCon *Con);
 *
 * PARAMETERS:
 *    Con [I] -- The connection to work on
 *
 * FUNCTION:
 *    This function checks to see if a connection is the error state.
 *    An error is when the connection is no long working (not a read/write
 *    error).
 *    This is mainly used after a SocketsCon_Connect() is called.  Because the
 *    tick function is used so the connect is nonblocking it can have an
 *    error while running the state machine and this function is used to
 *    check that.
 *
 * RETURNS:
 *    true -- The connection is not longer usable
 *    false -- The connection is not in the error state.
 *
 * SEE ALSO:
 *    SocketsCon_IsConnected()
 ******************************************************************************/
bool SocketsCon_HasError(struct SocketCon *Con)
{
    return Con->State==e_ConnectState_Error;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_IsConnected
 *
 * SYNOPSIS:
 *    bool SocketsCon_IsConnected(struct SocketCon *Con);
 *
 * PARAMETERS:
 *    Con [I] -- The connection to work on
 *
 * FUNCTION:
 *    This function checks to see if the connection state machine has goten
 *    to the connected state (ready to send/recv).
 *
 * RETURNS:
 *    true -- The connection is ready to send/recv
 *    false -- The connection is still being setup or is at an error state.
 *
 * SEE ALSO:
 *    SocketsCon_HasError()
 ******************************************************************************/
bool SocketsCon_IsConnected(struct SocketCon *Con)
{
    return Con->State==e_ConnectState_Connected;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_GetLastErrNo
 *
 * SYNOPSIS:
 *    int SocketsCon_GetLastErrNo(struct SocketCon *Con);
 *
 * PARAMETERS:
 *    Con [I] -- The connection to work on
 *
 * FUNCTION:
 *    This function gets the value of the errno that was recorded after
 *    the last read/write.
 *
 * RETURNS:
 *    errno
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
int SocketsCon_GetLastErrNo(struct SocketCon *Con)
{
    return Con->Last_errno;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_GetErrorCode
 *
 * SYNOPSIS:
 *    e_ConnectErrorType SocketsCon_GetErrorCode(struct SocketCon *Con);
 *
 * PARAMETERS:
 *    Con [I] -- The connection to work on
 *
 * FUNCTION:
 *    This function gets the error code for the socket system (instead of
 *    a system error number).
 *
 * RETURNS:
 *    e_ConnectError_AllOk -- We didn't have an error (it means we have an
 *                            unhandled error, or your calling this even though
 *                            nothing failed)
 *    e_ConnectError_gethostbynameFailed -- The call to gethostbyname() failed
 *    e_ConnectError_Failed2GetSocket --
 *    e_ConnectError_ConnectFailed --
 *    e_ConnectError_Failed2Connect --
 *    e_ConnectError_Failed2Getsockopt --
 *    e_ConnectError_Failed2Change2NonBlocking --
 *    e_ConnectError_SelectFailed --
 *    e_ConnectError_SSLConnectFailed --
 *    e_ConnectError_Failed2AllocSSL_CTX --
 *    e_ConnectError_ErrorSettingCertificate --
 *    e_ConnectError_ErrorSettingKeyFile --
 *    e_ConnectError_KeyDoesNotMatchTheCertificatePublicKey --
 *    e_ConnectError_ErrorSettingVerifyLocations --
 *    e_ConnectError_SSL_load_client_CA_fileFailed --
 *    e_ConnectError_CannotProperlyLoadCerts --
 *    e_ConnectError_Failed2AllocSSL --
 *    e_ConnectError_Failed2SetSSLFD --
 *    e_ConnectError_WriteTX_SOCKET_ERROR --
 *    e_ConnectError_ReadSocketError --
 *    e_ConnectError_Failed2Bind --
 *    e_ConnectError_AcceptError --
 *    e_ConnectError_AcceptTX_SOCKET_ERROR --
 *    e_ConnectErrorMAX
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
e_ConnectErrorType SocketsCon_GetErrorCode(struct SocketCon *Con)
{
    return Con->ErrorCode;
}

/*******************************************************************************
 * NAME:
 *    SocketsCon_GetSocketHandle
 *
 * SYNOPSIS:
 *    bool SocketsCon_GetSocketHandle(struct SocketCon *Con,
 *          t_ConSocketHandle *RetHandle);
 *
 * PARAMETERS:
 *    Con [I] -- The connection to work on
 *    RetHandle [O] -- The OS's handle to the socket being used for this
 *                     connection.
 *
 * FUNCTION:
 *    This function gets the underlying socket handle.  This is so you can
 *    wait on this handle.
 *
 * RETURNS:
 *    true -- Things socket is open and valid
 *    false -- The socket is not valid.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
bool SocketsCon_GetSocketHandle(struct SocketCon *Con,
        t_ConSocketHandle *RetHandle)
{
    if(Con->SocketFD<0)
        return false;

    *RetHandle=Con->SocketFD;

    return true;
}

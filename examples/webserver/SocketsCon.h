/*******************************************************************************
 * FILENAME: SocketsCon.h
 * 
 * PROJECT:
 *    Bitty HTTP
 *
 * FILE DESCRIPTION:
 *    This file has the functions that can be called to access the sockets
 *    and the definition of a socket handle.
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
#ifndef __SOCKETSCON_H_
#define __SOCKETSCON_H_

/***  HEADER FILES TO INCLUDE          ***/
#include <stdbool.h>
#include <stdint.h>

/***  DEFINES                          ***/

/***  MACROS                           ***/

/***  TYPE DEFINITIONS                 ***/
typedef enum
{
    e_ConnectError_AllOk=0,
    e_ConnectError_gethostbynameFailed,
    e_ConnectError_Failed2GetSocket,
    e_ConnectError_ConnectFailed,
    e_ConnectError_Failed2Connect,
    e_ConnectError_Failed2Getsockopt,
    e_ConnectError_Failed2Change2NonBlocking,
    e_ConnectError_SelectFailed,
    e_ConnectError_SSLConnectFailed,
    e_ConnectError_Failed2AllocSSL_CTX,
    e_ConnectError_ErrorSettingCertificate,
    e_ConnectError_ErrorSettingKeyFile,
    e_ConnectError_KeyDoesNotMatchTheCertificatePublicKey,
    e_ConnectError_ErrorSettingVerifyLocations,
    e_ConnectError_SSL_load_client_CA_fileFailed,
    e_ConnectError_CannotProperlyLoadCerts,
    e_ConnectError_Failed2AllocSSL,
    e_ConnectError_Failed2SetSSLFD,
    e_ConnectError_WriteTX_SOCKET_ERROR,
    e_ConnectError_ReadSocketError,
    e_ConnectError_Failed2Bind,
    e_ConnectError_AcceptError,
    e_ConnectError_AcceptTX_SOCKET_ERROR,
    e_ConnectErrorMAX
} e_ConnectErrorType;

typedef enum
{
    e_ConnectState_Idle=0,
    e_ConnectState_Connecting,
    e_ConnectState_Connected,
    e_ConnectState_Listening,
    e_ConnectState_Error,
    e_ConnectStateMAX
} e_ConnectStateType;

struct SocketCon
{
    e_ConnectStateType State;
    int SocketFD;
    bool ReadInProgress;
    int Last_errno;
    uint32_t TimeoutTS;
    e_ConnectErrorType ErrorCode;
};

typedef int t_ConSocketHandle;

/***  CLASS DEFINITIONS                ***/

/***  GLOBAL VARIABLE DEFINITIONS      ***/

/***  EXTERNAL FUNCTION PROTOTYPES     ***/
bool SocketsCon_InitSocketConSystem(void);
void SocketsCon_ShutdownSocketConSystem(void);
bool SocketsCon_InitSockCon(struct SocketCon *Con);
bool SocketsCon_Connect(struct SocketCon *Con,const char *ServerName,
        int portNo);
void SocketsCon_Tick(struct SocketCon *Con);
bool SocketsCon_Write(struct SocketCon *Con,const void *buf,int num);
int SocketsCon_Read(struct SocketCon *Con,void *buf,int num);
void SocketsCon_Close(struct SocketCon *Con);
bool SocketsCon_Listen(struct SocketCon *Con,const char *bindadd,int PortNo);
bool SocketsCon_Accept(struct SocketCon *Con,struct SocketCon *NewCon);
bool SocketsCon_HasError(struct SocketCon *Con);
bool SocketsCon_IsConnected(struct SocketCon *Con);
int SocketsCon_GetLastErrNo(struct SocketCon *Con);
e_ConnectErrorType SocketsCon_GetErrorCode(struct SocketCon *Con);
bool SocketsCon_EnableAddressReuse(struct SocketCon *Con,bool Enable);
bool SocketsCon_GetSocketHandle(struct SocketCon *Con,
        t_ConSocketHandle *RetHandle);

#endif

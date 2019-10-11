#pragma once

#include "SFServerNet.h" 
#include "SFNetwork_Def.h"
#include "SFTCPSendBuffer.h"
#include "SFUDPSendBuffer.h"

using namespace HbZeroAsioNet;

class ServerNetwork : public ServerNet<BasicTCPSendBuffer, BasicUDPSendBuffer>
{
public:
	ServerNetwork();
	virtual ~ServerNetwork(void);

	virtual bool ProcessSystemMsg( const INT32 nSessionID, const UINT16 packetID, const UINT16 bodySize, const char* pData ) override;
	
	
private:
	ERROR_NO SendTCP( const INT32 nSessionID, const CHAR* pPacket, const INT32 nDataLength );
	ERROR_NO SendUDP( const INT32 nSessionID, const CHAR* pPacket, const INT32 nDataLength );
	ERROR_NO SendUDP( const CHAR* szIP, const UINT16 nPort, const CHAR* pPacket, const INT32 nDataLength );
		
};


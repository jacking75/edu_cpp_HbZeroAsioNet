#include <iostream>

#include "SFDefaultFactoryNetworkComponent.h"
#include "SFDefaultPacketHandler.h"
#include "SFDefaultLog.h"


#include "ServerNetwork.h"
#include "Packet.h"



int main()
{
	NetworkConfig Config;
	Config.nServerID = 0;
	Config.bUseIPv6Address = false;
	Config.nTCPListenPort = 12345;
	Config.nMaxAcceptSessionPoolSize = 8;
	Config.nMaxConnectSessionPoolSize = 2;
	Config.nIOThreadCount = 4;
	Config.nMaxPacketSize = 1024;
	Config.nClientSessionReceiveBufferSize = 16000;
	Config.nClientSessionWriteBufferSize = 16000;
	Config.nServerSessionReceiveBufferSize = 30000;
	Config.nServerSessionWriteBufferSize = 30000;

	ConsoleLog logger;
	BasicPacketHandler packetHandler(&logger, Config);
	BasicFactoryNetworkComponent NetComponent;
	NetComponent.SetComponent(Config, &packetHandler);

	ServerNetwork Network;
	Network.Init( Config, &NetComponent );
	

	auto nRet = Network.Start();
	if( nRet == ERROR_NO::NONE ) 
	{ 
		logger.Write( LOG_TYPE::info, "Network Start" );
	} 
	else 
	{
		logger.Write( LOG_TYPE::error, "fail Network Start. Error(%d)", nRet );
		return 0;
	}

	
	bool bLoop = true;
	while( bLoop )
	{
		BasicPacketHandler::PACKET_DATA_INFO PacketDataInfo;
		
		auto bCanPacket = packetHandler.FrontPacket( PacketDataInfo );

		if( bCanPacket == false ) { 
			continue;
		}

		INT32 nSessionID = PacketDataInfo.nSessionID;
		char* pData = PacketDataInfo.pData;
		auto packetID = PacketDataInfo.PacketID();
		auto bodySize = PacketDataInfo.BodySize();

		switch(packetID)
		{
		case NETWORK_MESSAGE_ID::CHAT:
			{
				auto pPakcet = (ChatPacket*)pData;
				logger.Write( LOG_TYPE::trace, "CHAT. SessionID(%d), Message(%s)", nSessionID, pPakcet->szMessage );
			}
			break;

		default:
			{
				if( Network.ProcessSystemMsg( nSessionID, packetID, bodySize, pData ) == false )
				{
					logger.Write( LOG_TYPE::trace, "Receive. SessionID(%d), ID(%d), BodySize(%d)", nSessionID, packetID, bodySize);
				}
			}
			break;
		}

		packetHandler.PopPacket();
	}

	
	Network.Stop();

	return 0;
}


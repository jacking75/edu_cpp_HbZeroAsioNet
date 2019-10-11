#ifdef WIN32
#include <SDKDDKVer.h>
#endif

#include "ServerNetwork.h"
#include "SFDefaultPacketHandler.h"
#include "Packet.h"

using namespace HbZeroAsioNet;


ServerNetwork::ServerNetwork()
{
}

ServerNetwork::~ServerNetwork()
{
}

ERROR_NO ServerNetwork::SendTCP( const INT32 nSessionID, const CHAR* pPacket, const INT32 nDataLength )
{
	if( pPacket == nullptr ) { 
		return ERROR_NO::EMPTY_TCP_SEND_DATA;
	}

	auto pSession = m_pSessionMgr->FindServerSession( nSessionID );
	if( pSession == nullptr ) 
	{ 
		return ERROR_NO::NULL_SESSION_OBJECT;
	}

	if( pSession->IsTCPSendBlock() ) { 
		return ERROR_NO::BLOCKING_TCP_SEND;
	}
	
	pSession->PostSend( pPacket, nDataLength );
	
	return ERROR_NO::NONE;
}
	
ERROR_NO ServerNetwork::SendUDP( const INT32 nSessionID, const CHAR* pPacket, const INT32 nDataLength )
{
	if( pPacket == nullptr ) { 
		return ERROR_NO::EMPTY_UDP_SEND_DATA;
	}

	if( m_pUDPMgr == nullptr ) { 
		return ERROR_NO::NULL_UDP_SOCKET_MGR_OBJECT;
	}

	
	auto pSession = m_pSessionMgr->FindServerSession( nSessionID );
	
	if( pSession == nullptr ) {  
		return ERROR_NO::NULL_SESSION_OBJECT;
	}

	if( pSession->GetUDPPort() == 0 ) {
		return ERROR_NO::WRONG_SESSION_UDP_ADDRESS_INFO;
	}

	m_pUDPMgr->SendTo(pSession->GetIP(), pSession->GetUDPPort(), pPacket, nDataLength);
	return ERROR_NO::NONE;
}
	
ERROR_NO ServerNetwork::SendUDP( const CHAR* szIP, const UINT16 nPort, const CHAR* pPacket, const INT32 nDataLength )
{
	if( pPacket == nullptr ) { 
		return ERROR_NO::EMPTY_UDP_SEND_DATA;
	}

	if( m_pUDPMgr == nullptr ) { 
		return ERROR_NO::NULL_UDP_SOCKET_MGR_OBJECT;
	}

	m_pUDPMgr->SendTo(szIP, nPort, pPacket, nDataLength);
	return ERROR_NO::NONE;
}

bool ServerNetwork::ProcessSystemMsg( const INT32 nSessionID, const UINT16 packetID, const UINT16 bodySize, const char* pData )
{
	static INT32 nAcceptedCount = 0;
	
	switch(packetID)
	{
	case (UINT16)INNER_NETWORK_MESSAGE_ID::ACCEPT:
		{
			++nAcceptedCount;
			m_pLogger->Write( LOG_TYPE::info, "New Session. SessionID(%d). BodySize(%d), Count(%d)", nSessionID, bodySize, nAcceptedCount );
		}
		break;

	case (UINT16)INNER_NETWORK_MESSAGE_ID::CLOSE:
		{
			m_pLogger->Write( LOG_TYPE::info, "Close Session. SessionID(%d)", nSessionID );

			auto ErrorNo = ReUseSession( nSessionID );

			if( ErrorNo != ERROR_NO::NONE ) 
			{ 
				m_pLogger->Write( LOG_TYPE::info, "fail BindSocket. SessionID(%d)", nSessionID );
			}
		}
		break;

	case (UINT16)INNER_NETWORK_MESSAGE_ID::REQ_CLOSE_SESSION:
		{
			auto pPacket = (BasicPacketHandler::INNER_REQ_CLOSE_SESSION*)pData;
			m_pLogger->Write( LOG_TYPE::warn, "Must Disconnect Session. SessionID(%d), Error(%d)", nSessionID, pPacket->nError );

			Disconnect( nSessionID );
		}
		break;
		
	default:
		{
			m_pLogger->Write( LOG_TYPE::trace, "Receive. SessionID(%d), ID(%d), BodySize(%d)", nSessionID, packetID, bodySize );
		}
		return false;
	}

	return true;
}
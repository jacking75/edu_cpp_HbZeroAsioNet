#pragma once

#include <boost/circular_buffer.hpp>

#include "SFLock.h"
#include "SFMultiRingBuffer.h"


namespace HbZeroAsioNet
{
	//TODO: 외부에서 설정 값 받기. 단 아래 3개는 기본은 아닌 확장 설정 값으로 다룬다.
	const INT32 MAX_PACKET_BUFFER_COUNT = 4;
	const INT32 MAX_PACKET_BUFFER_SIZE  = 30000;
	const INT32 MAX_PACKET_COUNT		= 4096; 

	
	// 네트웍 패킷 인덱스
	enum class INNER_NETWORK_MESSAGE_ID : UINT16
	{
		ACCEPT = 1
		, CLOSE = 2
		, REQ_CLOSE_SESSION = 3
	};

	// 중요 : IO 스레드는 N개, 패킷을 처리하는 스레드가 1개인 경우에만 사용할 수 있음 !!!!!
	class BasicPacketHandler : public IPacketHandler
	{
	public:
#pragma pack(push, old)
#pragma pack(1)
		struct PACKET_HEADER
		{
			UINT16 nID;
			UINT16 nBodySize;
		};

		struct INNER_REQ_CLOSE_SESSION : PACKET_HEADER
		{
			INNER_REQ_CLOSE_SESSION()
			{
				nBodySize = sizeof(ERROR_NO);
			}

			ERROR_NO nError = ERROR_NO::NONE;
		};
#pragma pack(pop, old)

		const INT16 PACKET_HEADER_SIZE = sizeof(PACKET_HEADER);
		const INT16 MAX_PACKET_SIZE = 8096;//TODO 설정 정보를 통해서 정해져야 한다

	
		struct PACKET_DATA_INFO
		{
			INT32 nSessionID;
			INT16 nTotalSize;
			char* pData;
			UINT64 nBufferRotateCount;

			UINT16 PacketID() 
			{
				auto pPacketHeader = (PACKET_HEADER*)pData;
				return pPacketHeader->nID;
			}

			UINT16 BodySize()
			{
				auto pPacketHeader = (PACKET_HEADER*)pData;
				return pPacketHeader->nBodySize;
			}
		};


	public:
		BasicPacketHandler(ILogger* pLogger, NetworkConfig netConfig)
			: IPacketHandler( pLogger, netConfig)
			, m_Packets(MAX_PACKET_COUNT)
		{
			m_pLogger = pLogger;

			m_pReceiveBuffer = std::unique_ptr<MultiRingBuffer>(new MultiRingBuffer(m_pLogger));
			m_pReceiveBuffer->Allocate( MAX_PACKET_BUFFER_COUNT, MAX_PACKET_BUFFER_SIZE, MAX_PACKET_SIZE );

			Clear();
		}
		
		virtual ~BasicPacketHandler() 
		{
		}


		// 세션 초기화
		virtual void Clear() override 
		{
			__SCOPED_LOCK__ LOCK( m_Lock );

			m_pReceiveBuffer->Clear();
			m_Packets.clear();
		}

		// 처리할 수 있는 크기의 패킷인가?
		virtual bool CanMakePacket( const INT32 nSize, const char* pData ) override 
		{
			if( 0 >= nSize || nSize < PACKET_HEADER_SIZE ) {
				return false;
			}

			auto pPacketHeader = (PACKET_HEADER*)pData;
			
			if( nSize < (pPacketHeader->nBodySize+PACKET_HEADER_SIZE) ) {
				return false;
			}

			return true;
		}
	
		
		// 네트웍에서 받은 데이터 처리 - TCP
		virtual ERROR_NO ProcessRecvDataTCP( const INT32 nSessionID, const char* pData, INT32& nOutUseSize ) override 
		{
			auto nError			= ERROR_NO::NONE;
			auto pPacketHeader = (PACKET_HEADER*)pData;
			decltype(PACKET_HEADER_SIZE) nPacketSize = pPacketHeader->nBodySize + PACKET_HEADER_SIZE;
			
			nOutUseSize = nPacketSize;
			
					
			__SCOPED_LOCK__ LOCK( m_Lock );
			
			PACKET_DATA_INFO PacketDataInfo;
			PacketDataInfo.nSessionID	= nSessionID;
			PacketDataInfo.nTotalSize		= nPacketSize;
			
			nError = m_pReceiveBuffer->WriteBuffer( pData, nPacketSize );
			
			if( nError == ERROR_NO::NONE )
			{
				//m_pLogger->Write(LOG_TYPE::debug, "%s | SessionID(%d), PacketID(%d)", __FUNCTION__, nSessionID, pPacketHeader->nID);
				PacketDataInfo.pData = m_pReceiveBuffer->GetWriteBeforeData( nPacketSize );
				m_Packets.push_back( PacketDataInfo );
			}

			return nError;
		}
	
		// 네트웍에서 받은 데이터 처리 - UDP
		virtual ERROR_NO ProcessRecvDataUDP( const char* szIP, const unsigned short nPort, const INT32 nUDPID, const char* pData ) override 
		{
			auto nError			= ERROR_NO::NONE;
			//auto pPacket		= (PACKET*)pData;
			//decltype(PACKET_HEADER_SIZE) nPacketSize = pPacket->nBodySize + PACKET_HEADER_SIZE;
			

			// TODO :  등록되지 않은 리모트가 UDP 세션을 보낼 때와 등록된 리모트가 보낼 때를 분리해서 처리하기

			/* TCP의 패킷 처리 핸들링과 비슷하게 처리하면 된다
			__SCOPED_LOCK__ LOCK( m_Lock );
			
			PACKET_DATA_INFO PacketDataInfo;
			PacketDataInfo.nSessionID	= nSessionID;
			PacketDataInfo.pData		= m_pReceiveBuffer->GetData();
			PacketDataInfo.nSize		= nPacketSize;

			nError = m_pReceiveBuffer->WriteBuffer( pData, nPacketSize );
			
			if( nError == ERROR_NO::NONE )
			{
				m_Packets.push_back( PacketDataInfo );
			}*/

			return nError;
		}
	
	
		// 세션이 연결 되었음을 통보
		virtual void NotifyAccept( const INT32 nSessionID ) override 
		{
			PACKET_HEADER PacketHeader;
			PacketHeader.nID = (UINT16)INNER_NETWORK_MESSAGE_ID::ACCEPT;
			PacketHeader.nBodySize = 0;

			INT32 nOutUseSize = 0;
			ProcessRecvDataTCP( nSessionID, (char*)&PacketHeader, nOutUseSize );
		}
	
		/// 세션이 끊어졌음을 통보
		virtual void NotifyDisconnect( const INT32 nSessionID ) override 
		{
			PACKET_HEADER PacketHeader;
			PacketHeader.nID = (UINT16)INNER_NETWORK_MESSAGE_ID::CLOSE;
			PacketHeader.nBodySize = 0;

			INT32 nOutUseSize = 0;
			ProcessRecvDataTCP( nSessionID, (char*)&PacketHeader, nOutUseSize );
		}
	
		// 세션이 받기 작업이 실패했음을 통보
		virtual void NotifyReqCloseSession( const INT32 nSessionID, const ERROR_NO nError ) override 
		{
			INNER_REQ_CLOSE_SESSION Packet;
			Packet.nID = (UINT16)INNER_NETWORK_MESSAGE_ID::REQ_CLOSE_SESSION;
			Packet.nError = nError;
			
			INT32 nOutUseSize = 0;
			ProcessRecvDataTCP( nSessionID, (char*)&Packet, nOutUseSize );
		}



	
		bool FrontPacket( PACKET_DATA_INFO& PacketDataInfo )
		{
			__SCOPED_LOCK__ LOCK( m_Lock );

			if( m_Packets.empty() == false )
			{
				memcpy( &PacketDataInfo, &m_Packets[0], sizeof(PACKET_DATA_INFO) );
				return true;
			}

			return false;
		}

		void PopPacket()
		{
			__SCOPED_LOCK__ LOCK( m_Lock );

			if( m_Packets.empty() == false )
			{
				m_Packets.pop_front();
			}
		}


	
		


	protected: 
		
#ifdef WIN32 
		WinSpinCriticalSection m_Lock;
#else 
		StandardCppCriticalSection m_Lock;
#endif
		
		std::unique_ptr<MultiRingBuffer> m_pReceiveBuffer;
		
		boost::circular_buffer< PACKET_DATA_INFO > m_Packets;
		
	};
}

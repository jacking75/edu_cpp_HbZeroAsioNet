#pragma once

#include <functional>

// < Boost >
#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4100 )
#include <boost/asio.hpp>
#pragma warning( pop )
#else
#include <boost/asio.hpp>
#endif

#include <boost/bind.hpp>


#include "HandlerAllocator.h"
#include "SFInterface.h"

namespace HbZeroAsioNet 
{
	template< class TSendBuffer >  
	class UDPSocket 
	{
	public :
		UDPSocket()
		{
			m_pLogger			= nullptr;
			m_pPacketHandler	= nullptr;
			m_nMaxPacketBufferSize = 0;
		}

		virtual ~UDPSocket() {}

		std::function<INT32(const char*, const UINT16)> GetUDPIDFunc;


		ERROR_NO Init( const bool bUseIPv6Address, 
							UDPNetConfig Config, 
							IFactoryNetworkComponent* pFactoryNetworkComponent, 
							boost::asio::io_service* pIOService )
		{
			if( Config.nPort == 0 ) {
				return ERROR_NO::WRONG_UDP_PORT;
			}

			if( Config.nMaxPacketBufferSize <= 0 ) { 
				return ERROR_NO::WRONG_UDP_PACKET_BUFFER_SIZE;
			}
						
			if( pFactoryNetworkComponent == nullptr ) { 
				return ERROR_NO::NULL_UDP_NETWORK_COMPONENT_OBJECT;
			}


			m_pPacketHandler	= pFactoryNetworkComponent->GetPacketHandler();
			m_pLogger			= pFactoryNetworkComponent->GetLogger();


			bool bUseExternalIO = false;

			if( pIOService != nullptr )
			{
				bUseExternalIO = true;
				m_pIOService.reset( pIOService );
			}
			else
			{
				m_pIOService = std::unique_ptr<boost::asio::io_service>(new boost::asio::io_service);
			}


			try
			{
				if( bUseIPv6Address ) 
				{ 
					auto pSocket = new boost::asio::ip::udp::socket( *m_pIOService.get(), 
										boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v6(), Config.nPort) );
				
					m_pSocket =  std::unique_ptr<boost::asio::ip::udp::socket>( pSocket );

					m_pLogger->Write( LOG_TYPE::debug, "%s | UseIPv6Address", __FUNCTION__ );
				} 
				else 
				{
					auto pSocket = new boost::asio::ip::udp::socket( *m_pIOService.get(), 
										boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), Config.nPort) );

					m_pSocket = std::unique_ptr<boost::asio::ip::udp::socket>(pSocket);
				
					m_pLogger->Write( LOG_TYPE::debug, "%s | UseIPv4Address", __FUNCTION__ );
				}
			}
			catch (std::exception& e )
			{
				m_pLogger->Write( LOG_TYPE::warn, "%s | asio::ip::udp::socket Create. (%s)", __FUNCTION__, e.what() );
			}
	
			if( m_pSocket == nullptr ) 
			{ 
				m_pLogger->Write( LOG_TYPE::warn, "%s | NULL_UDP_SOCKET_OBJECT", __FUNCTION__ );
				return ERROR_NO::NULL_UDP_SOCKET_OBJECT;
			}

	
			m_nPort			= Config.nPort;
					
			m_pSendBuffer = std::unique_ptr<TSendBuffer>( new TSendBuffer(nullptr) );
			m_pSendBuffer->Allocate( Config.nWriteBufferLength, Config.nWriteBufferIndexCount );
		
			m_nMaxPacketBufferSize = Config.nMaxPacketBufferSize;
			m_pPacketBuffer = std::unique_ptr<char[]>( new char[m_nMaxPacketBufferSize] );


			RecvFrom();
		
			
			if( bUseExternalIO == false )
			{
				if( Config.nThreadCount == 0 )
				{
					Config.nThreadCount = (std::thread::hardware_concurrency() * 2) + 1;
				}

				for (INT32 i = 0; i < Config.nThreadCount; ++i)
				{
					m_UDPThreadList.emplace_back( std::thread( boost::bind(&boost::asio::io_service::run, m_pIOService.get()) ) );
				}
			}
	
			m_pLogger->Write( LOG_TYPE::info, "%s | Success. IsUseIPv6Address(%d), nPort(%u)", __FUNCTION__, bUseIPv6Address, Config.nPort);
			return ERROR_NO::NONE;
		}
	
		void SendTo( const char* szIP, const UINT16 nPort, const char* pPacket, const INT32 nPacketLen)
		{
			char* pSendPacket = m_pSendBuffer->GetPostBuffer( pPacket, nPacketLen );
		
			if( pSendPacket == nullptr ) 
			{
				return;
			}
	
			m_pSocket->async_send_to( boost::asio::buffer(pSendPacket, nPacketLen), 
										boost::asio::ip::udp::endpoint( boost::asio::ip::address::from_string(szIP), nPort ), 
										make_custom_alloc_handler( 
										m_Allocator,
											boost::bind( &UDPSocket::OnSendTo, this, 
												boost::asio::placeholders::error, 
												boost::asio::placeholders::bytes_transferred ) 	
												)
									);
		
		}

		UINT16 GetPort()						{ return m_nPort; }

		void StopProcessRecvData( bool bStop ) { m_bStopProcessRecvData = bStop; }
	 
		
	protected:
		void RecvFrom()
		{
			m_pSocket->async_receive_from
				( 
				boost::asio::buffer(m_pPacketBuffer.get(), m_nMaxPacketBufferSize), 
					m_SenderEndpoint, 
					make_custom_alloc_handler( 
							m_Allocator,
							boost::bind( &UDPSocket::OnRecvFrom, this, 
							boost::asio::placeholders::error, 
							boost::asio::placeholders::bytes_transferred ) 
					) 
				);
		}

		void Close()
		{
			if ( m_pSocket != nullptr && m_pSocket->is_open() )
			{
				m_bClose = true;
		
				boost::system::error_code ec;
				m_pSocket->close(ec);
			}
		}
		
		void ReleaseUDP()
		{
			m_pIOService->stop();

			for( auto& Thread : m_UDPThreadList )
			{
				if( Thread.joinable() ) { 
					Thread.join();
				}
			}

			m_UDPThreadList.clear();


			Close();

		
			if( m_pSocket != nullptr )
			{
				if( m_pSocket->is_open() ) 
				{
					m_pSocket->close(); 
				}
			}

			m_pPacketHandler = nullptr;
		}

		

		void UDPSocket::OnRecvFrom( const boost::system::error_code& error, const size_t _bytes_transferred )
		{
			if( error )
			{
				if( IsCriticalError( error.value() ) ) 
				{ 
					// 로그 남기기
				}
			}
			else
			{
				if( m_bStopProcessRecvData == false ) 
				{
					auto nUDPID = GetUDPIDFunc(m_SenderEndpoint.address().to_string().c_str(), m_SenderEndpoint.port());
					m_pPacketHandler->ProcessRecvDataUDP( m_SenderEndpoint.address().to_string().c_str(), m_SenderEndpoint.port(), nUDPID, m_pPacketBuffer.get() );
				}
			}

			RecvFrom();
		}
		
		void OnSendTo( const boost::system::error_code& error, const size_t bytes_transferred )
		{
			if( error ) 
			{ 
				m_pLogger->Write( LOG_TYPE::warn, "%s | Error(%s)(%d), RecvBytes(%d)", 
										__FUNCTION__, error.message().c_str(), error.value(), bytes_transferred );
				return;
			}
		}

		bool IsCriticalError( const INT32 nErrorCode )
		{
			if( WSA_IO_PENDING == nErrorCode || ERROR_IO_PENDING == nErrorCode || WSAENOBUFS == nErrorCode ) { 
				return false;
			}
			return true;
		}
	


		std::unique_ptr< boost::asio::io_service >  m_pIOService;
		
		boost::asio::ip::udp::endpoint m_SenderEndpoint;

		std::unique_ptr<boost::asio::ip::udp::socket> m_pSocket;
				
		std::vector< std::thread > m_UDPThreadList;	
				
		HandlerAllocator m_Allocator;

		IPacketHandler*	m_pPacketHandler;
		ILogger* m_pLogger;
	
		UINT16 m_nPort;
		
		bool m_bClose;
		
		bool m_bStopProcessRecvData; // 받기 작업을 중단한다(패킷 처리만 중단). 동기화 하지 않아도 된다.
	
		std::unique_ptr<TSendBuffer> m_pSendBuffer;
				
		INT32 m_nMaxPacketBufferSize;
		std::unique_ptr<char[]> m_pPacketBuffer;
		
	};
}

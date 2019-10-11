#pragma once

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4100 )
#include <boost/asio.hpp>
#pragma warning( pop )
#else
#include <boost/asio.hpp>
#endif

#include "Helper.h"
#include "SFUDPSessionMgr.h"
#include "SFUDPSocket.h"


namespace HbZeroAsioNet 
{
	template< class TSendBuffer >
	class UDPMgr
	{
	public:
		UDPMgr() {}
		virtual ~UDPMgr() {}


		ERROR_NO Create( UDPNetConfig Config, 
						IFactoryNetworkComponent* pFactoryNetworkComponent, 
						boost::asio::io_service* pIOService )
		{
			m_pUDPSessionMgr = std::unique_ptr<UDPSessionMgr>(new UDPSessionMgr);

			auto delegateGetUDPIDFunc = [&] (const char* szUDPIP, const UINT16 nUDPPort)
			{
				return m_pUDPSessionMgr->GetUDPID(szUDPIP, nUDPPort);
			};

			if( m_pUDPv4.get() != nullptr || m_pUDPv6.get() != nullptr ) { 
				return ERROR_NO::ALREADY_CREATED_UDP_SOCKET;
			}

			if( pFactoryNetworkComponent == nullptr ) { 
				return ERROR_NO::NULL_UDP_NETWORK_COMPONENT_OBJECT;
			}

			if( pFactoryNetworkComponent->GetLogger() == nullptr ) { 
				return ERROR_NO::NULL_UDP_LOGGER;
			}

			if( pFactoryNetworkComponent->GetPacketHandler() == nullptr ) { 
				return ERROR_NO::NULL_UDP_PACKET_HANDLER;
			}

			
			auto pLogger = pFactoryNetworkComponent->GetLogger();
			auto nErrorCode1 = ERROR_NO::NONE;
			
			if( Config.nUseIPAddressVersion != IP_ADDRESS_VERSION_TYPE::IPv6 )
			{
				m_pUDPv4 = std::unique_ptr<UDPSocket<TSendBuffer>>(new UDPSocket<TSendBuffer>);
				
				nErrorCode1 = CreateUDP( false, 
										 Config,  
										 m_pUDPv4.get(), 
										 pFactoryNetworkComponent, 
										 pIOService );

				if( nErrorCode1 != ERROR_NO::NONE ) 
				{ 
					pLogger->Write( LOG_TYPE::warn, "%s | Failed Create IPv4 UDP. Error(%d), Port(%u)", __FUNCTION__, nErrorCode1, Config.nPort );
				} 
				else 
				{ 
					pLogger->Write( LOG_TYPE::info, "%s | Create IPv4 UDP. Port(%u)", __FUNCTION__, Config.nPort );
				}

				m_pUDPv4->GetUDPIDFunc = delegateGetUDPIDFunc;
			}


			ERROR_NO nErrorCode2 = ERROR_NO::NONE;
			
			if( Config.nUseIPAddressVersion != IP_ADDRESS_VERSION_TYPE::IPv4 )
			{
				m_pUDPv6 =  std::unique_ptr<UDPSocket<TSendBuffer>>( new UDPSocket<TSendBuffer>);

				nErrorCode2 = CreateUDP( true, 
										 Config,  
										 m_pUDPv6.get(), 
										 pFactoryNetworkComponent, 
										 pIOService );

				if( nErrorCode1 != ERROR_NO::NONE ) 
				{ 
					pLogger->Write( LOG_TYPE::warn, "%s | Failed Create IPv6 UDP. Error(%d), Port(%u)", __FUNCTION__, nErrorCode2, Config.nPort );
				} 
				else 
				{ 
					pLogger->Write( LOG_TYPE::info, "%s | Create IPv6 UDP. Port(%u)", __FUNCTION__, Config.nPort );
				}
			}

			if( nErrorCode1 != ERROR_NO::NONE && nErrorCode2 != ERROR_NO::NONE ) 
			{
				if( nErrorCode1 == ERROR_NO::NULL_UDP_SOCKET_OBJECT || nErrorCode2 == ERROR_NO::NULL_UDP_SOCKET_OBJECT ) 
				{ 
					return ERROR_NO::NULL_UDP_SOCKET_OBJECT;
				}
				
				return ERROR_NO::FAIL_CREATE_UDP_SOCKET_OBJECT;
			}
	
			return ERROR_NO::NONE;
		}
		
		void SendTo( const std::string& strIP, const UINT16 nPort, const char* pPacket, const INT32 nPacketLen )
		{
			if( Helper::IsIPv6Address( (INT32)strIP.length(), strIP.c_str() ) ) 
			{
				m_pUDPv6->SendTo( strIP.c_str(), nPort, pPacket, nPacketLen );
			}
			else
			{
				m_pUDPv4->SendTo( strIP.c_str(), nPort, pPacket, nPacketLen );
			}
		}
			
		void StopProcessRecvData( const bool bStop )
		{
			if( m_pUDPv4 != nullptr ) { 
				m_pUDPv4->StopProcessRecvData( bStop );
			}

			if( m_pUDPv6 != nullptr ) { 
				m_pUDPv6->StopProcessRecvData( bStop );
			}
		}

		ERROR_NO SetUDPPacketCryptKey(const INT32 nSessionID, const UINT32 nCrypterKey)
		{
			return m_pSessionMgr->SetServerConnection(nSessionID);
		}


	protected:
		ERROR_NO CreateUDP( const bool bUseIPv6Address, 
							UDPNetConfig Config, 
							UDPSocket<TSendBuffer>* pUDP,
							IFactoryNetworkComponent* pFactoryNetworkComponent, 
							boost::asio::io_service* pIOService )
		{
			auto nErrorCode = ERROR_NO::NONE;
			
			nErrorCode = pUDP->Init( bUseIPv6Address, 
									Config, 
									pFactoryNetworkComponent, 
									pIOService );
			return nErrorCode;
		}
	
		

		std::unique_ptr<UDPSocket<TSendBuffer>> m_pUDPv4;		
		std::unique_ptr<UDPSocket<TSendBuffer>> m_pUDPv6;

		std::unique_ptr<UDPSessionMgr> m_pUDPSessionMgr;
	};
}

#pragma once

#include <memory>

#include "SFUDPMgr.h"
#include "SFServerSessionMgr.h"



namespace HbZeroAsioNet 
{	 
	template<class TTCPSendBuffer, class TUDPSendBuffer>
	class ServerNet
	{
	public: 
		ServerNet()
			: m_Acceptor( m_IOService ) 
			, m_IOWork( new boost::asio::io_service::work( m_IOService ) )
		{
			m_IsShutDown	= false;
			m_IsReadyTo		= false;
			memset( &m_Config, 0, sizeof(NetworkConfig) );

			m_pNetworkComponent = nullptr;
			m_pLogger			= nullptr;
		}

		virtual ~ServerNet()
		{
			Release();
		}

		// 초기화
		virtual ERROR_NO Init( NetworkConfig Config, IFactoryNetworkComponent* pNetworkComponent )
		{
			auto nErrorCode		= ERROR_NO::NONE;
			
			if( pNetworkComponent == nullptr || pNetworkComponent->GetLogger() == nullptr ) { 
				return ERROR_NO::NULL_NETWORK_COMPONENT_OBJECT;
			}

			if( Config.nMaxAcceptSessionPoolSize < 0 ) { 
				return ERROR_NO::WRONG_MAX_ACCEPT_SESSION_COUNT; 
			}
			
			
			m_pSessionMgr = std::unique_ptr<ServerSessionMgr<TTCPSendBuffer>>(new ServerSessionMgr<TTCPSendBuffer>);
			nErrorCode = m_pSessionMgr->Init( Config, pNetworkComponent );		
			if( nErrorCode != ERROR_NO::NONE ) { 
				return nErrorCode;
			}

			
			if( Config.nIOThreadCount <= 0 )
			{
				Config.nIOThreadCount = (INT16)(std::thread::hardware_concurrency() * 2) + 1;
		
				if( Config.nMaxAcceptSessionPoolSize < Config.nIOThreadCount ) { 
					Config.nIOThreadCount = Config.nMaxAcceptSessionPoolSize;
				}
			}

			
			m_Config				= Config;
			m_pNetworkComponent		= pNetworkComponent;
			m_pLogger				= pNetworkComponent->GetLogger();


			nErrorCode = InitAcceptor( m_Config.bUseIPv6Address, m_Config.nTCPListenPort, m_Config.nIOThreadCount );
		
			if( nErrorCode != ERROR_NO::NONE ) { 
				return nErrorCode;
			}
				
			m_IsReadyTo = true;

			return ERROR_NO::NONE;
		}

		virtual void Release()	
		{
			if( m_IsShutDown ) { 
				return;
			}

			m_IsShutDown = true;

			m_pSessionMgr->DisconnectAll();
			

			if( m_Acceptor.is_open() )
			{
				// 서버가 종료되는 순간에 Accept 작업을 하고 있는 경우를 막기 위해 아주 잠시 sleep을 건다.
				std::this_thread::sleep_for( std::chrono::milliseconds(3) );

				// IO 작업을 중단한다.
				m_IOWork.reset();

				m_Acceptor.close();
			}
		
		
			for( auto& IoThread : m_IoWorkThreadList )
			{
				if( IoThread.joinable() )  { 
					IoThread.join();
				}
			}

			m_IoWorkThreadList.clear();


			DestroyUDP();

			m_IsReadyTo = false;
		}
		
		// 시작 
		virtual ERROR_NO Start()
		{
			INT32 nPostAcceptSessionCount = m_Config.nMaxAcceptSessionPoolSize + m_Config.nIOThreadCount;
			if (auto err = AllSessionPostAccept(nPostAcceptSessionCount); err != ERROR_NO::NONE)
			{
				return err;
			}
		
			for( INT32 i = 0; i < m_Config.nIOThreadCount; ++i )
			{
				m_IoWorkThreadList.emplace_back( std::thread( boost::bind(&boost::asio::io_service::run, &m_IOService) ) );
			}

			return ERROR_NO::NONE;
		}

		// 중지
		virtual void Stop()
		{
			m_pLogger->Write(LOG_TYPE::info, "Network Stop");
			Release();
		}
	
		// 접속 하기. 동기식
		virtual ERROR_NO Connect( INT32& nSessionID, const char* pszIP, const UINT16 nPort )
		{
			nSessionID = -1;

			auto pSession = m_pSessionMgr->UnUseConnectSession();			
			if( pSession == nullptr ) {
				return ERROR_NO::NULL_SESSION_OBJECT;
			}
						
			if(auto nErrorCode = pSession->Connect(m_IOService, pszIP, nPort); nErrorCode != ERROR_NO::NONE )
			{
				pSession->SetUnUsed();
				pSession->Reset();
				return nErrorCode;
			}

			pSession->SetServerConnection();
			nSessionID = pSession->GetSessionID();
			return ERROR_NO::NONE;
		}
	
		// 접속 끊기
		virtual void Disconnect( const INT32 nSessionID )
		{
			if( m_IsReadyTo == false ) { 
				return;
			}

			
			auto pSession = m_pSessionMgr->FindServerSession( nSessionID );
			
			if( pSession != nullptr ) {
				pSession->Disconnect( SOCKET_CLOSE::SELF_DISCONNECT );
			}
		}
	
		// 세션 재 사용. 다시 Accept에 걸어 놓는다. UnSafe
		virtual ERROR_NO ReUseSession( const INT32 nSessionID )
		{
			if( m_IsShutDown || m_IsReadyTo == false ) { 
				return ERROR_NO::NONE;
			}

			
			auto* pSession = m_pSessionMgr->FindServerSession( nSessionID );			
			if( pSession == nullptr ) { 
				return ERROR_NO::NULL_SESSION_OBJECT;
			}

			auto nResult = ERROR_NO::NONE;
			if( nSessionID < START_SERVER_CONNECT_ID ) 
			{ 
				nResult = PostAccept( pSession );				
				if( nResult != ERROR_NO::NONE && nResult != ERROR_NO::NULL_SESSION_OBJECT ) 
				{ 
					pSession->SetUnUsed();
					pSession->Disconnect( SOCKET_CLOSE::BIND_FAILED );
				}
			} 
			else
			{
				pSession->SetUnUsed();
				pSession->Reset();
			}
			
			return nResult;
		}
				
		virtual ERROR_NO CreateUDP( UDPNetConfig Config, IFactoryNetworkComponent* pFactoryNetworkComponent )
		{
			DestroyUDP();
			
			m_pUDPMgr =  std::unique_ptr<UDPMgr<TUDPSendBuffer>>(new UDPMgr<TUDPSendBuffer>);										
			return m_pUDPMgr->Create( Config, pFactoryNetworkComponent, nullptr );
		}
				
		// 네트웍 라이브러리 용 메시지를 처리한다. 메시지를 처리하면 true 반환
		virtual bool ProcessSystemMsg( const INT32 nSessionID, const UINT16 packetID, const UINT16 bodySize, const char* pData ) = 0;

				
		// TCP 세션에 암호키 설정
		ERROR_NO SetTCPPacketCryptKey( const INT32 nSessionID, const UINT32 nCrypterKey) 
		{
			return m_pSessionMgr->SetTCPPacketCrypter(nSessionID, nCrypterKey);
		}
		
		// UDP 세션에 암호키 설정
		ERROR_NO SetUDPPacketCryptKey( const INT32 nSessionID, const UINT32 nCrypterKey) 
		{
			return m_pUDPSessionMgr->SetUDPPacketCrypter(nSessionID, nCrypterKey);
		}
		
		// 세션이 서버간 연결임을 설정
		ERROR_NO SetServerToServerConnection( const INT32 nSessionID) 
		{
			return m_pUDPMgr->SetServerConnection( nSessionID );
		}

		// 세션의 TCP 주소 얻기
		void GetTCPAddressInfo( const INT32 nSessionID, char* szTCPIP, UINT16& nTCPPort) 
		{
			m_pSessionMgr->GetTCPAddressInfo( nSessionID, szTCPIP, nTCPPort );
		}
		
		// 세션의 TCP를 블럭 시킨다.
		void SetTCPSendBlock( const INT32 nSessionID ) 
		{
			m_pSessionMgr->SetTCPSendBlock( nSessionID );
		}
			

		ServerSessionMgr<TTCPSendBuffer>* GetSessionMgr() { return m_pSessionMgr.get(); }
			

		
	protected:	
		ERROR_NO InitAcceptor( const bool bUseIPv6Address, const UINT16 nListenPort, const INT32 nBacklogCount )
		{
			if( nListenPort == 0 ) { 
				return ERROR_NO::WRONG_LISTEN_PORT;
			}

			boost::system::error_code error;
			boost::asio::ip::tcp::endpoint endpoint;
			
			if( bUseIPv6Address )
			{
				boost::asio::ip::tcp::endpoint endpoint6( boost::asio::ip::tcp::v6(), nListenPort );
				m_Acceptor.open( endpoint6.protocol(), error );
				endpoint = endpoint6;
			}
			else
			{
				boost::asio::ip::tcp::endpoint endpoint4( boost::asio::ip::tcp::v4(), nListenPort );
				m_Acceptor.open( endpoint4.protocol(), error );
				endpoint = endpoint4;
			}
	
			if( error ) 
			{
				m_pLogger->Write( LOG_TYPE::error, "%s | Open. error(%s)(%d)", __FUNCTION__, error.message().c_str(), error.value() );
				return ERROR_NO::FAIL_INIT_ACCEPT_OPEN;
			}

			
			m_Acceptor.set_option( boost::asio::ip::tcp::acceptor::reuse_address(true), error );

			if( error ) 
			{
				m_pLogger->Write( LOG_TYPE::error, "%s | set_option. error(%s)(%d)", __FUNCTION__, error.message().c_str(), error.value() );
				return ERROR_NO::FAIL_INIT_ACCEPT_OPTION;
			}


			m_Acceptor.bind( endpoint, error );
			
			if( error ) 
			{
				m_pLogger->Write( LOG_TYPE::error, "%s | bind error(%s)(%d)", __FUNCTION__, error.message().c_str(), error.value() );
				return ERROR_NO::FAIL_INIT_ACCEPT_BIND;
			}


			m_Acceptor.listen( nBacklogCount, error );
			
			if( error ) 
			{
				m_pLogger->Write( LOG_TYPE::error, "%s | listen error(%s)(%d)", __FUNCTION__, error.message().c_str(), error.value() );
				return ERROR_NO::FAIL_INIT_ACCEPT_LISTEN;
			}

			m_pLogger->Write( LOG_TYPE::info, "%s | isIPv6(%d)", __FUNCTION__, endpoint.protocol().family() == AF_INET6 ? true : false );
			return ERROR_NO::NONE;
		}

		ERROR_NO AllSessionPostAccept(const INT32 nPostAcceptSessionCount)
		{
			for (INT32 i = 0; i < nPostAcceptSessionCount; ++i)
			{
				auto pSession = m_pSessionMgr->FindServerSession(i);
				if (pSession == nullptr) {
					return ERROR_NO::NULL_SERVER_NET_INIT_SERVER_SESSION_OBJECT;
				}

				if (auto err = PostAccept(pSession); err != ERROR_NO::NONE)
				{
					return err;
				}
			}

			return ERROR_NO::NONE;
		}
		
		ERROR_NO PostAccept( ServerSession<TTCPSendBuffer>* pSession )
		{
			if( pSession == nullptr ) {
				return ERROR_NO::NULL_SESSION_OBJECT;
			}

			if( pSession->IsConnect() ) { 
				return ERROR_NO::FAIL_ACCEPT_SESSION_OPEN;
			}
	
			if( pSession->IsUsed() ) {
				return ERROR_NO::FAIL_ALREADY_POST_ACCEPT;
			}

			// 이것이 위에 있어야 한다. 이유는 m_Acceptor.async_accept(..) 뒤에 하면 
			// m_Acceptor.async_accept(..)내부에서 다른 스레드에서 이벤트가 호출되고 그것의 접속이 끊어지면 PostAccpet가 호출될 수도 있다.
			pSession->SetUsed();
			pSession->Reset();
			pSession->CreateSocket( m_IOService );
			
			m_Acceptor.async_accept( *pSession->GetSocket(), 
										boost::bind(&ServerNet::OnAccept, 
													this, 
													pSession, 
													boost::asio::placeholders::error
												) 
									);
		
			return ERROR_NO::NONE;
		}
		
		void OnAccept( ServerSession<TTCPSendBuffer>* pSession, const boost::system::error_code& error )
		{
			if( m_IsShutDown || pSession == nullptr)
			{
				return;
			}

			
			pSession->SetUnUsed();
	
			if( !error )
			{
				pSession->AcceptProcessing();
			}
			else
			{
				if( m_IsShutDown ) { 
					return;
				}

				m_pLogger->Write( LOG_TYPE::warn, "%s | Fail. [%d]. ErrMsg(%s), SessionID(%d)", 
							__FUNCTION__, error.value(), error.message().c_str(), pSession->GetSessionID() );
		
				pSession->Disconnect( SOCKET_CLOSE::ACCEPT_FAILED );
			}
		}
		
		void DestroyUDP()
		{
			m_pUDPMgr.reset(nullptr);
		}
		

		

		bool m_IsShutDown;		// 셧다운 여부
		bool m_IsReadyTo;		// 준비완료 여부

		NetworkConfig m_Config;	// 네트웍 설정 정보
		
		boost::asio::io_service							 m_IOService;
		std::unique_ptr< boost::asio::io_service::work > m_IOWork;
		boost::asio::ip::tcp::acceptor					 m_Acceptor;
		

		std::vector<std::thread> m_IoWorkThreadList;	// IO처리(정확하게는 IOCP)용 스레드 갯수

		std::unique_ptr<UDPMgr<TUDPSendBuffer>>  m_pUDPMgr;
		
		std::unique_ptr<ServerSessionMgr<TTCPSendBuffer>> m_pSessionMgr; // 네트웍에 (TCP로)접속된 세션처리
				
		IFactoryNetworkComponent* m_pNetworkComponent;	// 네트웍 컴포넌트 객체
		ILogger*					  m_pLogger;			// 로거 객체	
	};
}


#pragma once

#include "SFServerSession.h"

namespace HbZeroAsioNet 
{
	class ILogger;

	template<class TTCPSendBuffer>  
	class ServerSessionMgr
	{
	public:
		ServerSessionMgr() {}
		virtual ~ServerSessionMgr() {}
		
		ERROR_NO Init( const NetworkConfig Config, IFactoryNetworkComponent* pNetworkComponent )
		{
			const auto nMaxAcceptSessionCount = Config.nMaxAcceptSessionPoolSize;
			const auto nSpareAcceptSessionCount = Config.nIOThreadCount; 
			const auto nMaxConnectSessionCount = Config.nMaxConnectSessionPoolSize; 

			m_pNetworkComponent = pNetworkComponent;

			if( nMaxAcceptSessionCount <= 0 ) { 
				return ERROR_NO::WRONG_MAX_SESSION_COUNT;
			}

			// Accpet의 backlog 수 정도로 세션을 더 만들고 Accept에 걸도록 한다. 
			// MaxAcceptSessionCount 를 넘어선 연결은 애플리케이션에서 접속을 짜르도록 한다.
			m_MaxAcceptSessionCount = nMaxAcceptSessionCount + nSpareAcceptSessionCount;
			
			for (auto i = 0; i < m_MaxAcceptSessionCount; ++i)
			{
				auto pSession = new ServerSession<TTCPSendBuffer>;
				pSession->Create( i, 
								Config.nClientSessionReceiveBufferSize, 
								Config.nClientSessionWriteBufferSize, 
								Config.nMaxPacketSize, 
								m_pNetworkComponent->GetPacketHandler()  );
				
				m_pAcceptSessions.emplace_back(pSession);
			}
		
			
			// 서버에서 다른 서버로 연결할 때 사용할 세션을 풀에 넣는다.
			m_MaxConnectSessioncount = nMaxConnectSessionCount;
			
			if( m_MaxConnectSessioncount > 0 )
			{
				for( auto i = 0; i < m_MaxConnectSessioncount; ++i )
				{
					auto pSession = new ServerSession<TTCPSendBuffer>;
					pSession->Create( i, 
									Config.nServerSessionReceiveBufferSize, 
									Config.nServerSessionWriteBufferSize, 
									Config.nMaxPacketSize, 
									m_pNetworkComponent->GetPacketHandler()  );

					m_pConnectSessions.emplace_back(pSession);
				}
			}

			return ERROR_NO::NONE;
		}

		// 연결용 세션 중 사용하지 않는 것 찾기 
		ServerSession<TTCPSendBuffer>* UnUseConnectSession()
		{
			for( auto& pSession : m_pConnectSessions )
			{
				if( pSession->IsConnect() == false )
				{
					// 멀티스레드에서 호출했을 때 사용 예정 중인 세션을 동시 사용하는 것을 막기 위해 락을 건다
					__SCOPED_LOCK__ LOCK( m_Lock );
					
					if( pSession->IsUsed() == false)
					{
						pSession->SetUsed();
						return pSession.get();
					}
				}
			}

			return nullptr;
		}

		// 세션 검색
		ServerSession<TTCPSendBuffer>* FindServerSession(INT32 nSessionID)
		{
			if( nSessionID < 0 ) { 
				return nullptr;
			}

			if( nSessionID >= 0 && nSessionID < m_MaxAcceptSessionCount ) {
				return m_pAcceptSessions[ nSessionID ].get();
			}

			auto nServerSessionID = nSessionID - START_SERVER_CONNECT_ID;
			
			if( m_MaxConnectSessioncount > 0 && 
				(nServerSessionID >= 0 || nServerSessionID < m_MaxConnectSessioncount) 
			  ) 
			{ 
				return m_pConnectSessions[ nServerSessionID ].get();
			}
	
			return nullptr;
		}

		// 모든 세션의 연결을 끊는다
		void DisconnectAll()
		{
			for( auto& pSession : m_pAcceptSessions )
			{
				pSession->Disconnect( SOCKET_CLOSE::SELF_DISCONNECT );
			}
	
			for( auto& pSession : m_pConnectSessions )
			{
				pSession->Disconnect( SOCKET_CLOSE::SELF_DISCONNECT );
			}
		}

		// TCP 세션에 암호키 설정
		ERROR_NO SetTCPPacketCrypter( const INT32 nSessionID, const UINT32 nCrypterKey )
		{
			auto* pSession = FindServerSession(nSessionID);
			
			if( pSession == nullptr ) {
				return ERROR_NO::INVALID_SESSION;
			}

			if( nCrypterKey == 0 ) { 
				return ERROR_NO::INVALID_CRYPT_KEY;
			}

			pSession->SetTCPPacketCryptKey( nCrypterKey );
			
			return ERROR_NO::NONE;
		}
		
		// 세션이 서버간 연결임을 설정
		ERROR_NO SetServerConnection( const INT32 nSessionID )
		{
			auto pSession = FindServerSession( nSessionID );
			
			if( pSession == nullptr ) { 
				return ERROR_NO::INVALID_SESSION;
			}

			pSession->SetServerConnection();
			
			return ERROR_NO::NONE;
		}
		
		// 세션의 TCP를 블럭 시킨다.
		void SetTCPSendBlock( const INT32 nSessionID )
		{
			auto pSession = FindServerSession(nSessionID);
			
			if( pSession == nullptr ) {
				return;
			}

			pSession->SetTCPSendBlock();
		}

		void GetTCPAddressInfo(INT32 nSessionID, char* szTCPIP, USHORT& nTCPPort)
		{
			auto pSession = FindServerSession( nSessionID );
			
			if( pSession != nullptr && pSession->IsConnect() ) 
			{
				pSession->GetRemoteAddress( szTCPIP, nTCPPort );
			}
		}
		
		
	protected:
		void Destory()
		{
			m_pAcceptSessions.clear();
	
			m_pConnectSessions.clear();
		}


		INT32 m_MaxAcceptSessionCount; // 접속을 받을 최대 세션 수. 이 수 이상은 접속을 받지 않는다.		
		std::vector< std::unique_ptr<ServerSession<TTCPSendBuffer>> > m_pAcceptSessions; 

		INT32 m_MaxConnectSessioncount; // 다른 곳에 직접 연결할 때 사용할 세션 수
		std::vector< std::unique_ptr<ServerSession<TTCPSendBuffer>> > m_pConnectSessions;

#ifdef WIN32 
		WinSpinCriticalSection m_Lock;
#else 
		StandardCppCriticalSection m_Lock;
#endif
		
		IFactoryNetworkComponent* m_pNetworkComponent;
	};
}

#pragma once

#include <deque>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

#include "SFLock.h"


namespace HbZeroAsioNet 
{
	struct UDPSession
	{
		UDPSession() = default;
		~UDPSession() = default;

		void SetInfo(const INT32 nSessionID, const char* pszIP, const UINT16 nPort)
		{
			m_nSessionID = nSessionID;
			m_strIP = pszIP;
			m_nPort = nPort;
			m_strID = MakeID(pszIP, nPort);
			m_nCrypterKey = 0;
		}

		void GetUDPAddressInfo(char* szIP, UINT16& nPort)
		{
			strncpy_s(szIP, MAX_IP_STRING_LENGTH, m_strIP.c_str(), MAX_IP_STRING_LENGTH);
			nPort = m_nPort;
		}

		const INT32 GetSessionID() const { return m_nSessionID; }
		const std::string GetID() const { return m_strID; }

		static std::string MakeID(const char* pszIP, const UINT16 nPort)
		{
			char szAddress[256];
			sprintf_s(szAddress, 256, "%s+%u", pszIP, nPort);
			return std::string(szAddress);
		}



		INT32 m_nSessionID = -1;
		std::string m_strID;

		std::string m_strIP;
		UINT16 m_nPort = 0;

		UINT32 m_nCrypterKey = 0;
	};


	class UDPSessionMgr
	{
	public:
		// 컨테이너의 키를 선언
		typedef boost::multi_index::const_mem_fun<UDPSession, const INT32, &UDPSession::GetSessionID>	IDX_SESSIONID;
		typedef boost::multi_index::const_mem_fun<UDPSession, const std::string, &UDPSession::GetID>	IDX_ID;
		
		// 인덱싱 타입을 선언
		typedef struct indices : public boost::multi_index::indexed_by
			<
				boost::multi_index::hashed_unique<IDX_SESSIONID>
				, boost::multi_index::hashed_unique<IDX_ID>
			>
		{
			enum INDEX
			{
				IDX_SESSIONID = 0				
				, IDX_ID 
				, IDX_END		
			};
		} INDICES;

		typedef boost::multi_index_container<UDPSession*, indices> SessionContainer;

		typedef std::deque<UDPSession*> SessionPool;

	
		UDPSessionMgr() {}

		void Init( const INT32 nMaxSessionCount )
		{
			for( auto i = 0; i < nMaxSessionCount; ++i )
			{
				auto pSession = new UDPSession;
				m_SessionPool.push_back( pSession );
			}
		}

		virtual ~UDPSessionMgr() 
		{
			Unsafe_ClearAllSession();

			for( auto pSession : m_SessionPool ) { 
				delete pSession;
			}

			m_SessionPool.clear();
		}

		void Unsafe_ClearAllSession() 
		{
			for( auto pSession : m_UDPSessions ) 
			{ 
				Unsafe_ReutnToSessionPool( pSession );
			}

			m_UDPSessions.clear(); 
		}


		INT32 GetUDPID( const char* szUDPIP, const UINT16 nUDPPort )
		{
			INT32 nUDPID = -1;
			auto strKey = UDPSession::MakeID( szUDPIP, nUDPPort );

			__SCOPED_LOCK__ LOCK( m_Lock );

			auto pUDPSession = Unsafe_FindUDPSessionByKey( strKey );
			if( pUDPSession != nullptr ) { 
				nUDPID = pUDPSession->m_nSessionID;
			}
			
			return nUDPID;
		}

		// UDP 세션에 암호키 설정
		ERROR_NO SetUDPPacketCrypter(INT32 nSessionID, UINT32 nCrypterKey)
		{
			__SCOPED_LOCK__ LOCK( m_Lock );

			auto pSession = Unsafe_FindUDPSessionBySessionID( nSessionID );

			if( pSession == nullptr ) { 
				return ERROR_NO::FAIL_FIND_SESSION;
			}

			pSession->m_nCrypterKey = nCrypterKey;
	
			return ERROR_NO::NONE;
		}

		ERROR_NO AddSession( const char* pszIP, const UINT16 nPort, const INT32 nSessionID )
		{
			auto nIPLength = strnlen_s(pszIP, MAX_IP_STRING_LENGTH);

			if( nIPLength == 0 || nIPLength == MAX_IP_STRING_LENGTH ) {
				return ERROR_NO::WRONG_REMOTE_IP_STRING;
			}

			if( nPort == 0 ) { 
				return ERROR_NO::WRONG_REMOTE_UDP_PORT;
			}

			
			auto nError = ERROR_NO::NONE;
			std::string strID = UDPSession::MakeID(pszIP, nPort);

			__SCOPED_LOCK__ LOCK( m_Lock );
						
			if( Unsafe_FindUDPSessionByKey( strID ) == nullptr ) 
			{ 
				auto pSession = Unsafe_TakeFromSessionPool();
				pSession->SetInfo( nSessionID, pszIP, nPort );
				
				m_UDPSessions.insert( pSession );
			}
			else
			{
				nError = ERROR_NO::ADDRESS_ALREADY_IN_USE;
			}

			return nError;
		}

		void RemoveSession( const char* szUDPIP, const UINT16 nUDPPort )
		{
			auto strKey = UDPSession::MakeID( szUDPIP, nUDPPort );
			
			__SCOPED_LOCK__ LOCK( m_Lock );
			
			auto& Index = m_UDPSessions.get<indices::IDX_ID>();
			auto iter = Index.find( strKey );

			if (Index.end() != iter) 
			{
				Unsafe_ReutnToSessionPool( *iter );
				Index.erase( iter );
			}
		}

		void RemoveSession( const INT32 nSessionID )
		{
			__SCOPED_LOCK__ LOCK( m_Lock );
			
			auto& Index = m_UDPSessions.get<indices::IDX_SESSIONID>();
			auto iter = Index.find( nSessionID );

			if (Index.end() != iter) 
			{
				Unsafe_ReutnToSessionPool( *iter );
				Index.erase( iter );
			}
		}
		
		UDPSession* Unsafe_FindUDPSessionByKey( const std::string& strKey )
		{
			auto& Index = m_UDPSessions.get<indices::IDX_ID>();
			auto iter = Index.find( strKey );

			if (Index.end() != iter) 
			{
				auto pSession = *iter;
				return pSession;
			}

			return nullptr;
		}

		UDPSession* Unsafe_FindUDPSessionBySessionID( const INT32 nSessionID )
		{
			auto& Index = m_UDPSessions.get<indices::IDX_SESSIONID>();
			auto iter = Index.find( nSessionID );

			if (Index.end() != iter) 
			{
				auto pSession = *iter;
				return pSession;
			}

			return nullptr;
		}


		/*void GetUDPAddressInfo( const INT32 nSessionID, char* pszIP, INT16& nPort )
		{
		}*/

		

	protected:
		UDPSession* Unsafe_TakeFromSessionPool()
		{
			if( m_SessionPool.empty() ) { 
				return nullptr;
			}

			auto pSession = m_SessionPool[0];
			m_SessionPool.pop_front();

			return pSession;
		}

		void Unsafe_ReutnToSessionPool( UDPSession* pSession )
		{
			m_SessionPool.push_back( pSession );
		}


#ifdef WIN32 
		WinSpinCriticalSection m_Lock;
#else 
		StandardCppCriticalSection m_Lock;
#endif

		SessionContainer m_UDPSessions;
		SessionPool m_SessionPool;

	};

}


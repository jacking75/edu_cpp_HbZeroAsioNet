#pragma once

#include "SFTCPSocket.h"

namespace HbZeroAsioNet 
{
	template<class TCPSendBuffer>
	class ServerSession : public TCPSocket<TCPSendBuffer>
	{
	public:
		ServerSession()
		{
			m_nSessionID			= -1;
			SetUnUsed();
		}

		virtual ~ServerSession() { }
		
		void Create( const INT32 nSessionID, const INT32 nReceiveBufferMaxSize, const INT32 nWriteBufferMaxSize, const INT32 nPacketMaxSize, IPacketHandler* pPacketHandler )
		{
			m_nSessionID = nSessionID;
			m_pPacketHandler = pPacketHandler;
						
			
			auto pRecvBuffer = new RingBuffer(m_pPacketHandler->GetLogger());
			pRecvBuffer->Allocate(nReceiveBufferMaxSize, nPacketMaxSize);
			
			auto pSendBuffer = new TCPSendBuffer(m_pPacketHandler->GetLogger());
			pSendBuffer->Allocate(nWriteBufferMaxSize);
			
			TCPSocket::SetRecvSendBuffer(pRecvBuffer, pSendBuffer);
		}
		
		// 사용 후 접속을 끊었을 때 다시 사용하기 위해 초기화 상태로 재설정
		void Reset()
		{
			m_IsTCPSendBlock	 = false;
			m_IsClientConnection = true;

			TCPSocket::Init();

			m_nUDPPort = 0;
		}

		void SetUsed() { m_IsUsed = true; }
		void SetUnUsed() { m_IsUsed = false; }
		bool IsUsed()	{ return m_IsUsed; }

		/// TCP 패킷 보내기 중지 여부. true이면 TCP로 패킷을 보내지 않는다
		void SetTCPSendBlock()	{ m_IsTCPSendBlock = true; }		
		bool IsTCPSendBlock()	{ return m_IsTCPSendBlock; }

		bool IsConnect()		{ return TCPSocket::IsConnect(); }

		void SetTCPPacketCryptKey( const UINT32 nCryptKey ) { TCPSocket::SetTCPPacketCryptKey( nCryptKey ); }

		INT32 GetSessionID()	{ return m_nSessionID; }

		bool	IsClientConnection()	{ return  m_IsClientConnection; }
		void	SetServerConnection()	{ m_IsClientConnection = false; }
		void	SetClientConnection()	{ m_IsClientConnection = true; }

		boost::asio::ip::tcp::socket* GetSocket() { return TCPSocket::GetSocket(); }
		
		// Accpet 되었을 때 해야할 작업 처리
		void AcceptProcessing()
		{
			TCPSocket::SetSocketState( SOCKET_STATE::CONNECT );
			TCPSocket::SetSocketOption();
		
			TCPSocket::SetRemoteAddress();

			m_pPacketHandler->NotifyAccept( m_nSessionID );
		
			TCPSocket::PostRecv();
		}

		// 패킷을 보낸다.
		void PostSend( const char* pPacket, const INT32 nPacketLen ) 
		{ 
			TCPSocket::PostSend( pPacket, nPacketLen );
		}

		// UDP 주소 정보를 설정
		void SetUDPAddress( const char* pszIP, const UINT16 nPort )
		{
			m_nUDPPort = nPort;
			strncpy_s( m_szRemoteIP, pszIP, MAX_IP_STRING_LENGTH );
		}

		char* GetIP()			{ return m_szRemoteIP; }
		UINT16 GetUDPPort()		{ return m_nUDPPort; }



	protected:
		bool m_IsUsed;			
		bool m_IsClientConnection;	// 세션이 클라이언트 사이드 여부

		UINT16 m_nUDPPort;
		char m_szRemoteIP[MAX_IP_STRING_LENGTH];
	};
}


#pragma once

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
#include "SFLock.h"
#include "SFRingBuffer.h"

namespace HbZeroAsioNet 
{
	class ILogger;
	class IPacketHandler;
	class RingBuffer;
	class ISendBuffer;


	template<class TCPSendBuffer>  
	class TCPSocket
	{
	public:
		TCPSocket() {}
		virtual ~TCPSocket() {}
						
		void Init()
		{
			m_nCryptKey = 0;
			m_nSocketState = SOCKET_STATE::INIT;
			m_ErrorCount.Clear();

			memset(m_szRemoteIP, 0, MAX_IP_STRING_LENGTH);
			m_nRemotePort = 0;

			m_pReceiveBuffer->Clear();
			m_pSendBuffer->Clear();
			m_pPacketHandler->Clear();

			m_nLatestClosedTimeSec = 0;
		}

		// Asio의 TCP 소켓 객체 생성
		void CreateSocket( boost::asio::io_service& _IOService )
		{
			if (m_pSocket == nullptr)
			{
				m_pSocket = std::unique_ptr<boost::asio::ip::tcp::socket>(new boost::asio::ip::tcp::socket(_IOService));
			}
		}

		virtual ERROR_NO Connect( boost::asio::io_service& _IOService, const char* szIP, const UINT16 nTCPPort )
		{
			CreateSocket(_IOService);


			const auto nIPStringLen = strlen(szIP);

			if (nIPStringLen == 0 || nIPStringLen >= MAX_IP_STRING_LENGTH) {
				return ERROR_NO::WRONG_IP_ADDRESS;
			}

			if (nTCPPort == 0) {
				return ERROR_NO::WRONG_TCP_PORT;
			}

			if (m_pSocket == nullptr) {
				return ERROR_NO::NULL_TCP_SOCKET_OBJECT;
			}

			if (m_pSocket->is_open()) {
				return ERROR_NO::OPENED_SOCKERT;
			}

			if (m_nSocketState == SOCKET_STATE::CONNECT) {
				return ERROR_NO::SOCKET_ALREADY_STATE_OPEN;
			}


			try
			{
				boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(szIP), nTCPPort);
				m_pSocket->connect(endpoint);
			}
			catch (std::exception & error)
			{
				UNREFERENCED_PARAMETER(error);

				m_nSocketState = SOCKET_STATE::CONNECT;
				Close(SOCKET_CLOSE::CONNECT_FAILED);

				return ERROR_NO::FAIL_TRY_CONNECT;
			}

			m_nSocketState = SOCKET_STATE::CONNECT;
			SetSocketOption();

			PostRecv();

			return ERROR_NO::NONE;
		}

		// (주도적으로)접속을 끊는다. 내가 직접 접속을 끊을 때 호출
		virtual void Disconnect( const SOCKET_CLOSE nCase )
		{
			m_IsTCPSendBlock = false;
			Close(nCase);
		}
				
		// 접속을 끊는다.
		void Close( const SOCKET_CLOSE nCloseCase )
		{
			__SCOPED_LOCK__ LOCK(m_Lock);

			if (m_pSocket != nullptr)
			{
				if ((m_nSocketState == SOCKET_STATE::CONNECT ||
					nCloseCase == SOCKET_CLOSE::ACCEPT_FAILED) &&
					m_pSocket->is_open()
					)
				{
					m_nLatestCloseCase = nCloseCase;
					m_nSocketState = SOCKET_STATE::CLOSE;

					// shutdown을 하면 클라이언트에서 아주 빠르게 재 접속을 하면 connect 못하는 문제가 있다. 
					// 직접 접속을 짜를 때(SOCKET_CLOSE::DISCONNECT)만 사용한다
					if (nCloseCase == SOCKET_CLOSE::SELF_DISCONNECT) {
						m_pSocket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
					}

					// 소켓을 클로즈할 때 비동기 send,recv에서 소켓 작업이 취소 되면서 에러가 나올수 있으므로 무시한다.(10038)
					boost::system::error_code error;
					m_pSocket->close(error);
					time(&m_nLatestClosedTimeSec);

					if (SOCKET_CLOSE::CONNECT_FAILED != nCloseCase)
					{
						auto pLogger = m_pPacketHandler->GetLogger();
						pLogger->Write(LOG_TYPE::debug, "%s | Case(%d, %s), SessionID(%d)",
							__FUNCTION__, nCloseCase, STR_SOCKET_CLOSE_CASE[(INT32)nCloseCase], m_nSessionID);
					}
				}
			}

			if (nCloseCase != SOCKET_CLOSE::SELF_DISCONNECT)
			{
				m_pPacketHandler->NotifyDisconnect(m_nSessionID);
			}

			m_nSocketState = SOCKET_STATE::CLOSE;
		}
		
		// 데이터 보내기
		void PostSend( const char* pPacket, const INT32 nPacketLen )
		{
			if (m_pSocket != nullptr && m_nSocketState == SOCKET_STATE::CONNECT)
			{
				auto pSendPacket = m_pSendBuffer->GetPostBuffer(pPacket, nPacketLen);

				if (pSendPacket == nullptr) {
					return;
				}

				Send(pSendPacket, nPacketLen);
			}
		}
		
		// 데이터 받기
		void PostRecv()
		{
			if (m_pSocket == nullptr) {
				return;
			}

			auto pBuffer = m_pReceiveBuffer->GetWriteData();
			auto nStartPos = m_pReceiveBuffer->GetWriteOffset();

			m_pSocket->async_read_some
			(
				boost::asio::buffer(&pBuffer[nStartPos], m_pReceiveBuffer->GetWriteOnceMaxSize()),
				make_custom_alloc_handler(
					m_Allocator,
					boost::bind(&TCPSocket::OnRecv, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred)
				)
			);
		}
		
		// 소켓 상태
		void SetSocketState( const SOCKET_STATE nSocketState ) { m_nSocketState = nSocketState; }
		
		// 소켓 옵션 설정
		void SetSocketOption()
		{
			if (m_pSocket != nullptr)
			{
				// 링거는 소켓 종료가 빠르게 되도록 0 으로 설정합니다.
				m_pSocket->set_option(boost::asio::socket_base::linger(true, 0));
			}
		}
		
		void SetTCPPacketCryptKey( const UINT32 nCryptKey ) { m_nCryptKey = nCryptKey; }

		INT32 GetSessionID()							{ return m_nSessionID; }
		
		UINT32 GetCryptKey()							{ return m_nCryptKey; }
		
		boost::asio::ip::tcp::socket* GetSocket()		{ return m_pSocket.get(); }
		
		bool IsConnect()								{ return m_nSocketState == SOCKET_STATE::CONNECT ? true : false; }

		void SetRemoteAddress()
		{
			strncpy_s(m_szRemoteIP, MAX_IP_STRING_LENGTH, m_pSocket->remote_endpoint().address().to_string().c_str(), MAX_IP_STRING_LENGTH - 1);
			m_nRemotePort = m_pSocket->remote_endpoint().port();
		}
		
		void GetRemoteAddress( char* pszIP, UINT16& nPort )
		{
			strncpy_s(pszIP, MAX_IP_STRING_LENGTH, m_szRemoteIP, MAX_IP_STRING_LENGTH - 1);
			nPort = m_nRemotePort;
		}
				
		
		INT32 GetSendBufferMaxSize() { return m_pSendBuffer->GetBufferSize(); }
	
	

	protected:
		void SetRecvSendBuffer(RingBuffer* pRecvBuffer, TCPSendBuffer* pSendBuffer)
		{
			m_pReceiveBuffer = std::unique_ptr<RingBuffer>(pRecvBuffer);
			m_pSendBuffer = std::unique_ptr<TCPSendBuffer>(pSendBuffer);
		}

		// 진짜 데이터를 보낸다
		void Send( const char* pPacket, const INT32 nPacketLen )
		{
			// asio::async_write 실행 중에 소켓 접속이 끊어지면 핸들러 에러만 발생하지 시스템에 크래쉬는 발생하지 않을 것이라고 예상?
			// 만약 이상한 일이 발생한다면 예외처리를 하도록 한다
			if (m_pSocket == nullptr || m_nSocketState != SOCKET_STATE::CONNECT || pPacket == nullptr) {
				return;
			}

			boost::asio::async_write(
				*m_pSocket,
				boost::asio::buffer(pPacket, nPacketLen),
				make_custom_alloc_handler
				(
					m_Allocator,
					boost::bind(&TCPSocket::OnSend, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred)
				)
			);
		}

		// 데이터 보내기 완료 이벤트
		void OnSend( const boost::system::error_code& error, size_t bytes_transferred )
		{
			if (m_nSocketState != SOCKET_STATE::CONNECT) {
				return;
			}

			if (error)
			{
				//  이 에러들은 소켓이 앞에서 끊어진 경우이므로 에러를 그냥 무시한다.
				if (error.value() == WSAECONNABORTED || error.value() == WSAECONNRESET) {
					return;
				}

				// 가능하면 Send에서는 에러가 발생하면 짜르지 않도록 하는 것이 좋다. 
				// 이유는 Recv쪽에서 Close가 발생했을 때 서로 중복이 될 수 있기 때문이다
				if (IsCriticalError(error.value()) || IsSocketErrorMaxOver())
				{
					m_pPacketHandler->NotifyReqCloseSession(m_nSessionID, ERROR_NO::SESSION_SEND_IO_ERROR);
				}
			}
			else
			{
				if (bytes_transferred == 0) {
					return;
				}

				auto nPacketLen = 0;
				auto pSendPacket = m_pSendBuffer->GetNextPostBuffer((INT32)bytes_transferred, nPacketLen);

				if (pSendPacket != nullptr)
				{
					Send(pSendPacket, nPacketLen);
				}
			}
		}

		// 데이터 받기 완료 이벤트
		void OnRecv( const boost::system::error_code& error, size_t bytes_transferred )
		{
			if (error)
			{
				auto bIsMustSocketClose = true;

				if (bytes_transferred > 0)
				{
					if (IsCriticalError(error.value()) || IsSocketErrorMaxOver())
					{
						m_pPacketHandler->NotifyReqCloseSession(m_nSessionID, ERROR_NO::SESSION_RECV_IO_ERROR);
					}

					bIsMustSocketClose = false;
				}

				if (bIsMustSocketClose)
				{
					Disconnect(SOCKET_CLOSE::NETWORK_ON_CLOSE);
				}
			}
			else
			{
				if (bytes_transferred > 0)
				{
					m_pReceiveBuffer->IncreaseWriteOffset((INT32)bytes_transferred);
					m_pReceiveBuffer->BufferFilledThenRotate(m_pReceiveBuffer->GetWriteOnceMaxSize());

					OnProecssRecvData();
				}

				PostRecv();
			}
		}
	
		// 받은 데이터 처리하기
		void OnProecssRecvData()
		{
			auto bLoop = true;

			while (bLoop)
			{
				if (m_pPacketHandler->CanMakePacket(m_pReceiveBuffer->GetReadableDataSize(), m_pReceiveBuffer->GetReadData()) == false) {
					break;
				}

				auto nOutPacketSize = 0;
				auto nError = m_pPacketHandler->ProcessRecvDataTCP(m_nSessionID, m_pReceiveBuffer->GetReadData(), nOutPacketSize);

				if (nError != ERROR_NO::NONE)
				{
					m_pPacketHandler->NotifyReqCloseSession(m_nSessionID, nError);
					bLoop = false;
				}

				m_pReceiveBuffer->IncreaseReadOffset(nOutPacketSize);
			}
		}

		// 위험한 에러 여부 조사. 접속을 끊어야 할 에러인 경우 true 반환
		bool IsCriticalError( const INT32 nErrorCode )
		{
			if (WSA_IO_PENDING == nErrorCode || ERROR_IO_PENDING == nErrorCode || WSAENOBUFS == nErrorCode) {
				return false;
			}
			return true;
		}

		// 소켓과 관련된 에러 발생 최대 횟수를 넘었나? 넘은 경우 true 반환
		bool IsSocketErrorMaxOver()
		{
			auto bIsLimitOver = false;
			auto nCurTime = time(NULL);

			auto nInitTime = TCP_SOCKET_ERROR_COUNT_REINIT_TIME_CLIENT_SIDE;

			if (m_bClientConnection == false) {
				nInitTime = TCP_SOCKET_ERROR_COUNT_REINIT_TIME_SERVER_SIDE;
			}


			auto nLimitCount = TCP_BEARABLE_MAX_SOCKET_ERROR_COUNT_CLIENT_SIDE;

			if (m_bClientConnection == false) {
				nLimitCount = TCP_BEARABLE_MAX_SOCKET_ERROR_COUNT_SERVER_SIDE;
			}


			__SCOPED_LOCK__ LOCK(m_Lock);

			auto nSpandTime = nCurTime - m_ErrorCount.nLatestTime;

			if (nSpandTime > nInitTime) {
				m_ErrorCount.nErrorCount = 0;
			}

			m_ErrorCount.nLatestTime = nCurTime;
			++m_ErrorCount.nErrorCount;

			if (m_ErrorCount.nErrorCount > nLimitCount) {
				bIsLimitOver = true;
			}

			return bIsLimitOver;
		}

		

		std::unique_ptr<boost::asio::ip::tcp::socket> m_pSocket;

		INT32 m_nSessionID = 0;				// 세션 아이디
		SOCKET_STATE m_nSocketState;	// 소켓 상태
		
		UINT32 m_nCryptKey;				// 암호키
		bool m_bClientConnection;		// 클라이언트 세션 여부. true이면 클라이언트 접속
	
		char m_szRemoteIP[ MAX_IP_STRING_LENGTH ];
		UINT16 m_nRemotePort;

		HandlerAllocator m_Allocator;		// 이벤트 완료 핸들러 등록용 메모리 풀
		
		IPacketHandler*	m_pPacketHandler;	// 패킷 처리 핸들러
		
		std::unique_ptr<RingBuffer> m_pReceiveBuffer;

		std::unique_ptr<TCPSendBuffer> m_pSendBuffer;  
				
#ifdef WIN32 
		WinSpinCriticalSection m_Lock;
#else 
		StandardCppCriticalSection m_Lock;
#endif
		SocketErrorCount m_ErrorCount;		// 에러 발생 횟수
		
		SOCKET_CLOSE	m_nLatestCloseCase;		// 가장 마지막에 클로즈된 경우
		time_t			m_nLatestClosedTimeSec; // 가장 최근에 접속이 끊어진 시간
		
		bool m_IsTCPSendBlock;					// 데이터 보내기 중단 여부. true이면 데이터를 보내지 않는다
	};
}

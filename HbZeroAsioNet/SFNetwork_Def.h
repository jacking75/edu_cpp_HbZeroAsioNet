#pragma once


#ifndef WIN32
	#define INT16 	short
	#define UINT16 	unsigned short
	#define INT32 	int
	#define UINT32 	unsigned int
#else
	#include <SDKDDKVer.h>

	#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용은 Windows 헤더에서 제외합니다.
	#include <Windows.h>
#endif

namespace HbZeroAsioNet 
{
	const INT32 MAX_SPIN_LOCK_COUNT = 4000;		// Windows 뮤텍스 설정에서 사용

	const INT32 MAX_IP_STRING_LENGTH = 24;		// IP 문자열 길이. IPv6도 사용 가능

	const INT32	START_SERVER_CONNECT_ID = 100000001; // 서버에서 다른 서버에 연결할 세션의 세션 시작 ID

	const INT32 TCP_SOCKET_ERROR_COUNT_REINIT_TIME_CLIENT_SIDE	= 5;	// 5초. TCP 소켓에서 에러가 발생했을 때 횟수를 초기화 시키는 이전 시간과의 차이
	const INT32 TCP_BEARABLE_MAX_SOCKET_ERROR_COUNT_CLIENT_SIDE	= 5;	// TCP 소켓 에러가 이 횟수 이상 발생하면 더 이상 사용하기 힘들다고 판단하는 값
	const INT32 TCP_SOCKET_ERROR_COUNT_REINIT_TIME_SERVER_SIDE	= 5;	
	const INT32 TCP_BEARABLE_MAX_SOCKET_ERROR_COUNT_SERVER_SIDE	= 20;	

	
	enum class IP_ADDRESS_VERSION_TYPE : INT16
	{
		IPv4			= 0
		, IPv6			= 1
		, IPv4and6		= 2
	};


	enum class SOCKET_STATE : INT16
	{
		INIT		= 0
		, CONNECT	
		, CLOSE		
	};
	

	// 소켓 접속을 끊은 이유
	enum class SOCKET_CLOSE : INT16
	{
		NONE				= 0
		, ACCEPT_FAILED				
		, CONNECT_FAILED
		, BIND_FAILED
		, NETWORK_ON_CLOSE		// 상대방이 접속을 끊었음
		, SELF_DISCONNECT		// 내가 접속을 끊음
	};
	
	const char STR_SOCKET_CLOSE_CASE[][32] = { "None", "ACCEPT_FAILED", "CONNECT_FAILED", "BIND_FAILED", "NETWORK_ON_CLOSE", "SELF_DISCONNECT" };


	enum class ERROR_NO : INT16
	{
		NONE							= 0

		, WRONG_IP_ADDRESS				= 11
		, WRONG_TCP_PORT				= 12
		, NULL_TCP_SOCKET_OBJECT		= 14 
		, OPENED_SOCKERT				= 15
		, SOCKET_ALREADY_STATE_OPEN		= 16
		, FAIL_TRY_CONNECT				= 17
		, WRONG_MAX_ACCEPT_SESSION_COUNT = 18
		, NULL_NETWORK_COMPONENT_OBJECT	= 19


		, NULL_SESSION_OBJECT			= 31
		, ADDRESS_ALREADY_IN_USE		= 32

		, WRONG_UDP_PORT				= 51
		, NULL_UDP_SOCKET_OBJECT		= 52
		, ALREADY_CREATED_UDP_SOCKET	= 53
		, FAIL_CREATE_UDP_SOCKET_OBJECT = 54
		, NULL_UDP_SESSION_MGR_OBJECT	= 55
		, NULL_UDP_PACKET_HANDLER_OBJECT = 56
		, WRONG_UDP_PACKET_BUFFER_SIZE  = 57
		, NULL_UDP_NETWORK_COMPONENT_OBJECT = 58
		, NULL_UDP_LOGGER				= 59
		, NULL_UDP_PACKET_HANDLER		= 60

		, INVALID_SESSION				= 71
		, INVALID_CRYPT_KEY				= 72
		, WRONG_MAX_SESSION_COUNT		= 73
			
		, WRONG_LISTEN_PORT				= 81
		, FAIL_INIT_ACCEPT_OPEN			= 82
		, FAIL_INIT_ACCEPT_OPTION		= 83 
		, FAIL_INIT_ACCEPT_BIND			= 84
		, FAIL_INIT_ACCEPT_LISTEN		= 85  
		, FAIL_ACCEPT_SESSION_OPEN		= 86 
		, FAIL_ALREADY_POST_ACCEPT		= 87
		, NULL_SERVER_NET_INIT_SERVER_SESSION_OBJECT	= 88 
		, NULL_BIND_TCP_SESSION_MGR_OBJECT	= 89

		, EMPTY_TCP_SEND_DATA			= 111
		, BLOCKING_TCP_SEND				= 112
		, EMPTY_UDP_SEND_DATA			= 114
		, NULL_UDP_SOCKET_MGR_OBJECT	= 115 
		, WRONG_SESSION_UDP_ADDRESS_INFO = 116

		, WRONG_COPY_DATA_SIZE			= 131
		, REMAINING_BUFFER_IS_TOO_MANY	= 132
		, OVER_FLOW_BUFFER				= 133
		, WRONG_WRITE_MARK_POS			= 134

		, SESSION_RECV_IO_ERROR			= 171
		, SESSION_SEND_IO_ERROR			= 172
		, SESSION_FAIL_RECV_DATA		= 173

		, WRONG_REMOTE_IP_STRING		= 301
		, WRONG_REMOTE_UDP_PORT			= 302
		, FAIL_FIND_SESSION				= 303
		, FAIL_GET_LOCAL_IP				= 304
		, EMPTY_SESSION_BY_CONNECT		= 305

		, CLIENT_CONFIG_WRONG_USE_SESSION_COUNT	= 501
	};
	

	struct SocketErrorCount
	{
		void Clear()
		{
			nLatestTime = 0;
			nErrorCount = 0;
		}
		time_t nLatestTime;
		INT32 nErrorCount;
	};

	
	// 서버의 네트웍 설정 정보
	struct NetworkConfig 
	{
		INT32	nServerID;							// 서버 ID
		
		bool	bUseIPv6Address;					// IPv6 사용 여부
		UINT16	nTCPListenPort;						// TCP 포트
		
		INT16	nMaxAcceptSessionPoolSize;			// 접속을 받은 세션 수
		INT16	nMaxConnectSessionPoolSize;			// 원격에 접속할 세션의 수
		INT16	nIOThreadCount;						// IO 작업 스레드 개수

		INT32	nMaxPacketSize;		
		INT32	nClientSessionReceiveBufferSize;	// 클라이언트 세션(서버에 연결된 클라이언트)의 받기 버퍼의 크기
		INT32	nClientSessionWriteBufferSize;		// 클라이언트 세션의 보내기 버퍼의 크기(하나 당)
		INT32	nServerSessionReceiveBufferSize;	// 서버 세션(서버와 서버끼리 연결된 세션)의 받기 버퍼의 크기
		INT32	nServerSessionWriteBufferSize;		// 서버 세션의 보내기 버퍼의 개수(하나 당)
	};

	struct UDPNetConfig 
	{
		IP_ADDRESS_VERSION_TYPE nUseIPAddressVersion;	// 사용할 IPv 주소 버전
		UINT16					nPort;		

		INT32					nMaxPacketBufferSize;	// 패킷 버퍼의 크기
		INT32					nWriteBufferIndexCount; // 쓰기 버퍼의 인덱스(Row) 수
		INT32					nWriteBufferLength;		// 쓰기 버퍼의 크기

		INT32					nThreadCount;			// IO 쓰레드 개수
	};


	
}


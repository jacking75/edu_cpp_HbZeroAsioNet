#pragma once

#include <stdio.h>

#ifdef WIN32
	#pragma warning (disable: 4100)

	#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용은 Windows 헤더에서 제외합니다.
	#include <Windows.h>
#endif

#include "SFNetwork_Def.h"


namespace HbZeroAsioNet 
{
	//Logger
	const INT32 MAX_LOG_STRING_LENGTH = 256;

	enum class LOG_TYPE : INT16 
	{
		trace		= 1
		, debug		= 2
		, warn		= 3
		, error		= 4
		, info		= 5
	};
	

	class ILogger 
	{
	public:
		ILogger() {}
		virtual ~ILogger() {}

		virtual void Write( const LOG_TYPE nType, const char* pFormat, ... )
		{
			char szText[ MAX_LOG_STRING_LENGTH ];

			va_list args;
			va_start( args,pFormat );
			vsprintf_s( szText, MAX_LOG_STRING_LENGTH, pFormat,args );
			va_end( args );

			switch( nType )
			{
			case LOG_TYPE::info :
				Info( szText );
				break;
			case LOG_TYPE::error :
				Error( szText );
				break;
			case LOG_TYPE::warn :
				Warn( szText );
				break;
			case LOG_TYPE::debug :
				Debug( szText );
				break;
			case LOG_TYPE::trace :
				Info( szText );
				break;
			default:
				break;
			}
		}

		
	protected:
		virtual void Error( const char* pText ) = 0;
		virtual void Warn( const char* pText ) = 0;
		virtual void Debug( const char* pText ) = 0;
		virtual void Trace( const char* pText ) = 0;
		virtual void Info( const char* pText ) = 0;
					
	};




	class IPacketHandler
	{
	public:
		IPacketHandler(ILogger* pLogger, NetworkConfig netConfig)
		{
			m_pLogger = pLogger;
			m_NetConfig = netConfig;
		}

		virtual ~IPacketHandler() {}

		ILogger* GetLogger() { return m_pLogger; }


		virtual void Clear() {}

		// 처리할 수 있는 크기의 패킷인가?
		virtual bool CanMakePacket(const INT32 nSize, const char* pData) = 0;

		// 네트웍에서 받은 데이터 처리 - TCP
		virtual ERROR_NO ProcessRecvDataTCP(const INT32 nSessionID, const char* pData, INT32& nOutUseSize) = 0;

		// 네트웍에서 받은 데이터 처리 - UDP
		virtual ERROR_NO ProcessRecvDataUDP(const char* szIP, const unsigned short nPort, const INT32 nUDPID, const char* pData) = 0;


		// 세션이 연결 되었음을 통보
		virtual void NotifyAccept(const INT32 nSessionID) = 0;

		// 세션이 끊어졌음을 통보
		virtual void NotifyDisconnect(const INT32 nSessionID) = 0;

		// 세션 종료를 요청
		virtual void NotifyReqCloseSession(const INT32 nSessionID, const ERROR_NO nError) = 0;



	protected:
		ILogger* m_pLogger;
		NetworkConfig m_NetConfig;
	};



	class IFactoryNetworkComponent
	{
	public:
		IFactoryNetworkComponent() {}
		virtual ~IFactoryNetworkComponent() {}

		virtual IPacketHandler* GetPacketHandler() = 0;

		virtual ILogger* GetLogger() = 0;

	};
}


#pragma once

//#include "SFILog.h"
//
//namespace HbZeroAsioNet 
//{
//	const INT32 MAX_TCP_SEND_RESERVATION_COUNT = 256;
//
//	class ISendBuffer
//	{
//	public:
//		ISendBuffer( ILog* pLogger )
//		{
//			m_pLogger = pLogger;
//		}
//
//		virtual ~ISendBuffer()
//		{
//		}
//
//		virtual void Allocate( const INT32 nBufferLength, const INT32 nRowCount=0, const INT32 nMaxSendCount=MAX_TCP_SEND_RESERVATION_COUNT ) = 0;
//		
//		virtual void Clear() = 0;
//		
//		virtual CHAR* GetPostBuffer( const CHAR* pData, const INT32 nLength ) = 0;
//		
//		virtual CHAR* GetNextPostBuffer( const INT32 nTransferred, INT32& nNextPostLength ) = 0;
//
//		INT32 GetBufferSize() { return m_nBufferSize; }
//
//
//
//	protected:
//		virtual void ReleaseBuffer() = 0;
//		virtual void Release() = 0;
//
//
//		ILog* m_pLogger;
//
//#ifdef WIN32 
//		WinSpinCriticalSection m_Lock;
//#else 
//		StandardCppCriticalSection m_Lock;
//#endif
//
//		INT32 m_nBufferSize;
//	};
//}

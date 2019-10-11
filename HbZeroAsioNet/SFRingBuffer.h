#pragma once

#include "SFInterface.h"


namespace HbZeroAsioNet 
{
	class RingBuffer
	{
	public:
		RingBuffer( ILogger* pLogger )				
		{  
			m_pLogger					= pLogger;

			m_pBuffer1					= nullptr; 
			m_pBuffer2					= nullptr;

			m_nWriteOffset				= 0;
			m_nReadOffset				= 0;

			m_nWriteOnceMaxSize			= 0;
			m_nBufferSize				= 0;
			m_nPlusExtraBufferLength	= 0;
		
			m_IsDebugging				= false;
		}

		virtual ~RingBuffer()	
		{
			Release();
		}

		void Allocate( const INT32 nBufferSize, const INT32 nWriteOnceMaxSize )
		{
			Release();
		
			m_nBufferSize		= nBufferSize;
			m_nWriteOnceMaxSize	= nWriteOnceMaxSize;

			// 버퍼를 한바퀴 돌 때 할당한 버퍼를 벗어나는 경우를 대비해서 실제 버퍼는 좀 더 크게 잡는다
			m_nPlusExtraBufferLength = m_nBufferSize + (m_nWriteOnceMaxSize * 2);

			m_pBuffer1 = new char[ m_nPlusExtraBufferLength ];
			m_pBuffer2 = new char[ m_nPlusExtraBufferLength ]; 

			Clear();
		}

		void Release()
		{
			delete[] m_pBuffer1;
			m_pBuffer1 = nullptr;

			delete[] m_pBuffer2;
			m_pBuffer2 = nullptr;

			m_nPlusExtraBufferLength = m_nBufferSize = 0;
		}

		void Clear()
		{
			m_nWriteOffset	= 0;
			m_nReadOffset	= 0;
		}

		ERROR_NO WriteBuffer( const char* pSourceData, const INT16 nDataSize )
		{
			if( nDataSize > m_nWriteOnceMaxSize )
			{
				m_pLogger->Write( LOG_TYPE::error, "%s | nDataSize(%d), WriteOnceMaxSize(%d)", __FUNCTION__, nDataSize, m_nWriteOnceMaxSize );
				return ERROR_NO::WRONG_COPY_DATA_SIZE;
			}

			
			ERROR_NO nError = BufferFilledThenRotate( nDataSize );

			if( nError != ERROR_NO::NONE ) { 
				return nError;
			}

						
			if( m_nWriteOffset < 0 || m_nWriteOffset >= m_nBufferSize || 
				m_nPlusExtraBufferLength < (m_nWriteOffset+nDataSize) 
			 )
			{
				m_pLogger->Write( LOG_TYPE::error, "%s | m_nPlusExtraBufferLength(%d), WriteOffset(%d), nDataSize(%d)", 
													__FUNCTION__, m_nPlusExtraBufferLength, m_nWriteOffset, nDataSize );
				return ERROR_NO::WRONG_WRITE_MARK_POS;
			}

			memcpy( &m_pBuffer1[ m_nWriteOffset ], pSourceData, nDataSize );
			m_nWriteOffset += nDataSize;

			return ERROR_NO::NONE;
		}

		ERROR_NO BufferFilledThenRotate( const INT32 nWriteSize )
		{
			auto nRemainBufferLen = m_nBufferSize - m_nWriteOffset;
			
			if( nRemainBufferLen < nWriteSize )
			{
				decltype(m_nWriteOffset) nNextReadLength = m_nWriteOffset - m_nReadOffset;
				
				if( nNextReadLength > 0 )
				{
					memcpy( m_pBuffer2, &m_pBuffer1[m_nReadOffset], nNextReadLength );
					
					decltype(m_pBuffer1) pTemp = m_pBuffer1;
					m_pBuffer1 = m_pBuffer2;
					m_pBuffer2 = pTemp;
					
					m_nReadOffset	= 0;
					m_nWriteOffset = nNextReadLength;
										
					if( m_IsDebugging ) { 
						m_pLogger->Write( LOG_TYPE::debug, "%s | Turn 1. WriteSize(%d), WriteOffset(%d)", __FUNCTION__, nWriteSize, m_nWriteOffset );
					}
				}
				else if( nNextReadLength == 0 )
				{
					m_nWriteOffset = 0;
					m_nReadOffset	= 0;

					if( m_IsDebugging ) { 
						m_pLogger->Write( LOG_TYPE::debug, "%s | Turn 2. WriteSize(%d), WriteOffset(%d)", __FUNCTION__, nWriteSize, m_nWriteOffset );
					}
				}
				else
				{
					m_pLogger->Write( LOG_TYPE::error, "%s | ReadOffset(%d), WriteOffset(%d)", __FUNCTION__, m_nReadOffset, m_nWriteOffset );
				}
			}

			return ERROR_NO::NONE;
		}

		INT32 GetBufferSize()							{ return m_nBufferSize; }
		
		INT32 GetReadableDataSize()							{ return m_nWriteOffset - m_nReadOffset; }
		
		char* GetWriteData()								{ return &m_pBuffer1[ m_nWriteOffset ]; }
		char* GetReadData()									{ return &m_pBuffer1[ m_nReadOffset ]; }

		void IncreaseReadOffset( const INT32 nSize )	{ m_nReadOffset += nSize; }
		void IncreaseWriteOffset( const INT32 nSize )	{ m_nWriteOffset += nSize; }
		
		INT32 GetReadOffset()							{ return m_nReadOffset; }
		INT32 GetWriteOffset()							{ return m_nWriteOffset; }
		
		INT32 GetWriteOnceMaxSize()						{ return m_nWriteOnceMaxSize; }

		void SetDebuging()								{ m_IsDebugging = true; }

		
	protected:
		
		char* m_pBuffer1;				// 버퍼
		char* m_pBuffer2;

		INT32 m_nWriteOffset;			// 쓰기 위치
		INT32 m_nReadOffset;			// 읽은 위치

		INT32 m_nWriteOnceMaxSize;		// 한번에 버퍼에 쓸 수 있는 최대 크기
		INT32 m_nBufferSize;			// 설정된 버퍼의 크기
		INT32 m_nPlusExtraBufferLength;	// 여분의 크기가 포함된 버퍼의 크기(실제 이 크기로 버퍼를 할당한다)
		
		ILogger* m_pLogger;				// 로거
		
		bool m_IsDebugging;				// 디버깅 중 여부
	};
}


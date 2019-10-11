#pragma once

#include <vector>
#include <memory>

#include "SFInterface.h"


namespace HbZeroAsioNet 
{
	class MultiRingBuffer
	{
	public:
		MultiRingBuffer( ILogger* pLogger )				
		{  
			m_pLogger					= pLogger;

			m_nWriteOffset				= 0;
			
			m_nWriteOnceMaxSize			= 0;
			
			m_nBufferSize				= 0;
			
			m_IsDebugging				= false;
		}

		virtual ~MultiRingBuffer()	
		{
			Release();
		}

		void Allocate( const INT32 nBufferCount, const INT32 nBufferSize, const INT32 nWriteOnceMaxSize )
		{
			Release();
		
			m_nBufferSize		= nBufferSize;
			m_nWriteOnceMaxSize	= nWriteOnceMaxSize;

			for( INT32 i = 0; i < nBufferCount; ++i )
			{
				m_BufferSet.emplace_back(std::unique_ptr<char[]>(new char[m_nBufferSize]));
				//m_BufferSet.emplace_back( std::move( std::unique_ptr<char[]>( new char[m_nBufferSize] ) ) );
			}
			
			Clear();
		}

		void Release()
		{
			m_BufferSet.clear();
			m_nBufferSize = 0;
		}

		void Clear()
		{
			m_nWriteOffset	= 0;
			m_nCurUseBufferIndex = 0;
		}

		ERROR_NO WriteBuffer( const char* pSourceData, const INT16 nDataSize )
		{
			if( nDataSize > m_nWriteOnceMaxSize )
			{
				m_pLogger->Write( LOG_TYPE::error, "%s | nDataSize(%d), WriteOnceMaxSize(%d)", __FUNCTION__, nDataSize, m_nWriteOnceMaxSize );
				return ERROR_NO::WRONG_COPY_DATA_SIZE;
			}

			
			BufferFilledThenRotate( nDataSize );
												
			if( m_nWriteOffset < 0 || m_nWriteOffset >= m_nBufferSize ||
				m_nBufferSize < (m_nWriteOffset+nDataSize)
			 )
			{
				m_pLogger->Write( LOG_TYPE::error, "%s | BufferSize(%d), WriteOffset(%d), nDataSize(%d)", 
													__FUNCTION__, m_nBufferSize, m_nWriteOffset, nDataSize );
				return ERROR_NO::WRONG_WRITE_MARK_POS;
			}

			memcpy( &m_BufferSet[m_nCurUseBufferIndex][ m_nWriteOffset ], pSourceData, nDataSize );
			m_nWriteOffset += nDataSize;

			return ERROR_NO::NONE;
		}

		void BufferFilledThenRotate( const INT32 nWriteSize )
		{
			auto nRemainBufferLen = m_nBufferSize - m_nWriteOffset;
			
			if( nRemainBufferLen < nWriteSize ) 
			{ 
				m_nWriteOffset = 0;
				++m_nCurUseBufferIndex;

				if( m_nCurUseBufferIndex >= (INT32)m_BufferSet.size() ) 
				{ 
					m_nCurUseBufferIndex = 0;
				}	
			}
		}

		char* GetWriteData() { 
			return &m_BufferSet[ m_nCurUseBufferIndex ][ m_nWriteOffset ]; 
		}

		char* GetWriteBeforeData( const INT32 nWriteSize ) { 
			return &m_BufferSet[m_nCurUseBufferIndex ][ m_nWriteOffset-nWriteSize ]; 
		}
		
		void SetDebuging()								{ m_IsDebugging = true; }

		
	protected:
		
		INT32 m_nCurUseBufferIndex;
		std::vector< std::unique_ptr<char[]> > m_BufferSet;

		INT32 m_nBufferSize;			// 설정된 버퍼의 크기

		INT32 m_nWriteOffset;			// 쓰기 위치
		INT32 m_nWriteOnceMaxSize;
		
		ILogger* m_pLogger;				// 로거
		
		bool m_IsDebugging;				// 디버깅 중 여부
	};
}
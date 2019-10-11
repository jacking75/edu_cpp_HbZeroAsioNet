#pragma once

#include <deque>
#include <vector>
#include <memory>

#include "SFInterface.h"
#include "SFLock.h"


namespace HbZeroAsioNet 
{
	class BasicUDPSendBuffer
	{
	public:
		BasicUDPSendBuffer( ILogger* pLogger )
		{
			m_nBufferRowCount = 0;
			Clear();
		}

		virtual ~BasicUDPSendBuffer()
		{
			Release();
		}
		
		void Clear()  
		{
			m_nWriteMarkPosList.assign( m_nBufferRowCount, 0 );
			m_nUseBufferRowIndex = 0;
		}
		
		void Allocate(const INT32 nBufferLength, const INT32 nRowCount = 0)
		{
			ReleaseBuffer();

			m_nBufferSize = nBufferLength;
			m_nBufferRowCount = nRowCount;

			for( INT32 i = 0; i < m_nBufferRowCount; ++ i )
			{
				m_BufferList.emplace_back(new char[m_nBufferSize]);
				m_nWriteMarkPosList.push_back( 0 );
			}

			m_nUseBufferRowIndex = 0;
		}

		CHAR* GetPostBuffer( const CHAR* pData, const INT32 nLength )  
		{
			char* pBufferPointer = nullptr;
	
			// UDP는 연결된 세션 별로 버퍼를 가지고 있지 않으므로 이전에 보냈던 패킷이 아직 
			// 완료가 되었는지 조사하지 않는다(특정 세션이 제대로 받지 못하고 있다면 다른 세션에게 영향을 주므로). 
			// 그러므로 버퍼의 크기를 좀 크게 잡는 것이 좋다
			__SCOPED_LOCK__ LOCK( m_Lock );
	
			if( m_nUseBufferRowIndex >= m_nBufferRowCount ) { 
				m_nUseBufferRowIndex = 0;
			}
	
			auto nWriteMarkPos = m_nWriteMarkPosList[ m_nUseBufferRowIndex ];
			
			if( (nWriteMarkPos+nLength) > m_nBufferSize ) { 
				nWriteMarkPos = 0;
			}

			if( (nWriteMarkPos+nLength) <= m_nBufferSize ) 
			{ 
				CopyMemory( &m_BufferList[m_nUseBufferRowIndex].get()[nWriteMarkPos], pData, nLength );
				pBufferPointer = &m_BufferList[m_nUseBufferRowIndex].get()[nWriteMarkPos];

				m_nWriteMarkPosList[ m_nUseBufferRowIndex ] = nWriteMarkPos + nLength;
				++m_nUseBufferRowIndex;
			}
			else 
			{
				m_pLogger->Write( LOG_TYPE::warn, "%s | Row(%d), WriteMarkPos(%d), Length(%d)", 
											__FUNCSIG__, m_nUseBufferRowIndex, nWriteMarkPos, nLength );
				return nullptr;
			}

			return pBufferPointer;
		}

		CHAR* GetNextPostBuffer( const INT32 nTransferred, INT32& nNextPostLength )  
		{
			UNREFERENCED_PARAMETER( nTransferred );
			UNREFERENCED_PARAMETER( nNextPostLength );
			return nullptr;
		}



	protected:

		void Release()  
		{
			ReleaseBuffer();
			Clear();
		}

		void ReleaseBuffer()  
		{
			m_nBufferSize = 0;
			m_nBufferRowCount = 0;

			m_BufferList.clear();
			m_nWriteMarkPosList.clear();
		}


		ILogger* m_pLogger;

#ifdef WIN32 
		WinSpinCriticalSection m_Lock;
#else 
		StandardCppCriticalSection m_Lock;
#endif

		INT32 m_nBufferSize;

		INT32 m_nBufferRowCount;
		std::vector<std::unique_ptr<char>> m_BufferList;

		INT32 m_nUseBufferRowIndex;
		std::vector< INT32 > m_nWriteMarkPosList;
	};
}


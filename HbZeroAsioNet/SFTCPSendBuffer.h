#pragma once


#include <deque>
#include <boost/circular_buffer.hpp>
//#include "SFNetwork_Def.h"
#include "SFInterface.h"


namespace HbZeroAsioNet 
{
	class BasicTCPSendBuffer
	{
	public:
		BasicTCPSendBuffer( ILogger* pLogger )
		{
			m_nBufferSize = 0;
			
			Clear();
		}

		virtual ~BasicTCPSendBuffer()
		{
			Release();
		}

		void Allocate( const INT32 nBufferLength) 
		{
			ReleaseBuffer();

			m_nBufferSize	= nBufferLength;
			m_pBuffer		= std::unique_ptr<char[]>( new char[m_nBufferSize] );
			
			m_nWriteMarkPos = 0;
		}

		void Clear()  
		{
			__SCOPED_LOCK__ LOCK( m_Lock );

			m_nWriteMarkPos			= 0;
			m_bIsSending			= false;
			m_nRemainPacketLength	= 0;
			
			m_SendPackets.clear();
		}


		char* GetPostBuffer( const char* pData, const INT32 nLength )  
		{
			// IO 완료는 되지 않았는데 계속 패킷 전송을 요청하게 되면 이전 것을 덮어 쓸 수도 있음 -_-;

			auto bFailed = false;
			char* pBufferPointer = NULL;


			__SCOPED_LOCK__ LOCK( m_Lock );
	
			if( m_nBufferSize < (m_nWriteMarkPos+nLength) ) 
			{ 
				if( (nLength+m_nRemainPacketLength) > m_nWriteMarkPos ) 
				{ 
					/// 서비스 할 때는 로그는 빼자!!
					m_pLogger->Write( LOG_TYPE::warn, "%s | nWriteMarkPos(%d), nLength(%d), nRemainPacketLength(%d)", 
													__FUNCTION__, m_nWriteMarkPos, nLength, m_nRemainPacketLength );
					bFailed = true;
				}
				else
				{
					m_nWriteMarkPos = 0;
				}
			}

			if( bFailed == false )
			{
				if( m_bIsSending == false ) 
				{
					pBufferPointer = &m_pBuffer[ m_nWriteMarkPos ];
					m_bIsSending = true;
				} 
				else 
				{
					m_SendPackets.push_back( std::pair< INT32, INT32 >(m_nWriteMarkPos, nLength) );
				}

				CopyMemory( &m_pBuffer[ m_nWriteMarkPos ], pData, nLength );
				m_nWriteMarkPos			+= nLength;
				m_nRemainPacketLength	+= nLength;
			}

			return pBufferPointer;
		}

		char* GetNextPostBuffer( const INT32 nTransferred, INT32& nNextPostLength )   
		{
			char* pBufferPointer = nullptr;
			nNextPostLength = 0;

			__SCOPED_LOCK__ LOCK( m_Lock );
			
			m_nRemainPacketLength -= nTransferred;

			if( m_SendPackets.empty() ) 
			{ 
				m_bIsSending = false;
			} 
			else 
			{
				m_bIsSending = true;
				auto sendpacket = m_SendPackets.front();
				pBufferPointer = &m_pBuffer[ sendpacket.first ];
				nNextPostLength = sendpacket.second;

				m_SendPackets.pop_front();
			}
			
			return pBufferPointer;
		}



	protected:
		
		void ReleaseBuffer()  
		{
			m_nBufferSize = 0;
			m_pBuffer.reset(nullptr);
		}

		void Release()    
		{
			ReleaseBuffer();
			Clear();
		}


		ILogger* m_pLogger;

#ifdef WIN32 
		WinSpinCriticalSection m_Lock;
#else 
		StandardCppCriticalSection m_Lock;
#endif

		INT32 m_nBufferSize;

		INT32 m_nWriteMarkPos;
		INT32 m_nRemainPacketLength;
		
		std::unique_ptr<char[]> m_pBuffer; 

		//TODO 컨테이너를 사용하지 않고 구현해본다
		std::deque< std::pair<INT32, INT32> > m_SendPackets; //버퍼위치, 크기 

		bool m_bIsSending;
	};
}


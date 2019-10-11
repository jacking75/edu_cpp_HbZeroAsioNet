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

#include "SFNetwork_Def.h"

namespace HbZeroAsioNet 
{
	class Helper 
	{
	public:
		static bool GetLocalIP(const bool bUseIPv6Address, boost::asio::io_service& IOServic)
		{
			char szHostName[256] = { 0, };
			
			boost::asio::ip::tcp::resolver resolver(IOServic);
			boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(),""); 
			boost::asio::ip::tcp::resolver::iterator it = resolver.resolve(query); 
			
			while( it != boost::asio::ip::tcp::resolver::iterator() ) 
			{ 
				boost::asio::ip::address addr=(it++)->endpoint().address(); 
				
				if( bUseIPv6Address && addr.is_v6() ) 
				{ 
					strncpy_s(szHostName, 256, addr.to_string().c_str(), 256 - 1);
				} 
				else if( bUseIPv6Address == false && addr.is_v6() == false ) 
				{
					strncpy_s(szHostName, 256, addr.to_string().c_str(), 256 - 1);
				}
			}
			
			return false;
		}

		// IP 주소가 IPv6 방식인지 조사
		static bool IsIPv6Address( const INT32 nIPStringLenght, const char* pszIP )
		{
			if( nIPStringLenght < 5 ) { 
				return false;
			}

			// IP 주소 문자열에 ':'이 있으면 이건 IPv6 주소이다. ^^;
			// IPv6은 최소 5자리부터는 ':'가 있고, IPv4의 최소 길이는 5자를 넘어서고, IPv6의 경우 5자리가 안되는 경우는 앞에 ':'이 있다.
			for( int i = 0; i < 5; ++i )
			{
				if( pszIP[i] == 58 ) { // ':'
					return true;
				}
			}

			return false;
		}
	};
}

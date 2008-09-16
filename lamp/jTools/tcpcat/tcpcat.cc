/*	=========
 *	tcpcat.cc
 *	=========
 */

// Standard C++
#include <string>

// Standard C/C++
#include <cerrno>
#include <cstdlib>

// POSIX
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Iota
#include "iota/strings.hh"

// Nucleus
#include "Nucleus/Convert.h"

// POSeven
#include "POSeven/FileDescriptor.hh"

// Orion
#include "Orion/Main.hh"


namespace tool
{
	
	namespace NN = Nucleus;
	namespace p7 = poseven;
	namespace O = Orion;
	
	
	static struct in_addr ResolveHostname( const char* hostname )
	{
		hostent* hosts = gethostbyname( hostname );
		
		if ( !hosts || h_errno )
		{
			std::string message = "Domain name lookup failed: ";
			
			message += NN::Convert< std::string >( h_errno );
			message += "\n";
			
			p7::write( p7::stderr_fileno, message.data(), message.size() );
			
			O::ThrowExitStatus( 1 );
		}
		
		in_addr addr = *(in_addr*) hosts->h_addr;
		
		return addr;
	}
	
	int Main( int argc, iota::argv_t argv )
	{
		if ( argc != 3 )
		{
			p7::write( p7::stderr_fileno, STR_LEN( "Usage:  tcpcat <host> <port>\n" ) );
			
			return 1;
		}
		
		const char* hostname = argv[1];
		const char* port_str = argv[2];
		
		p7::fd_t sock = p7::fd_t( socket( PF_INET, SOCK_STREAM, IPPROTO_TCP ) );
		
		short port = std::atoi( port_str );
		
		struct in_addr ip = ResolveHostname( hostname );
		
		struct sockaddr_in inetAddress = { 0 };
		
		inetAddress.sin_family = AF_INET;
		inetAddress.sin_port   = htons( port );
		inetAddress.sin_addr   = ip;
		
		p7::throw_posix_result( connect( sock, (const sockaddr*) &inetAddress, sizeof (sockaddr_in) ) );
		
		while ( true )
		{
			enum { blockSize = 4096 };
			char data[ blockSize ];
			
			std::size_t bytesToRead = blockSize;
			
			int received = read( sock, data, bytesToRead );
			
			if ( received == 0 )
			{
				break;
			}
			else if ( received == -1 )
			{
				std::perror( "tcpcat" );
				
				return 1;
			}
			
			(void) io::write( p7::stdout_fileno, data, received );
		}
		
		return 0;
	}
	
}

namespace Orion
{
	
	int Main( int argc, iota::argv_t argv )
	{
		return tool::Main( argc, argv );
	}
	
}


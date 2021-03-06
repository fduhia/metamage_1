/*	========
 *	httpd.cc
 *	========
 */

// Standard C/C++
#include <cctype>
#include <cstdio>
#include <cstdlib>

// Standard C
#include <stdlib.h>

// POSIX
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

// iota
#include "iota/strings.hh"

// gear
#include "gear/hexidecimal.hh"

// plus
#include "plus/hexidecimal.hh"
#include "plus/var_string.hh"

// poseven
#include "poseven/extras/pump.hh"
#include "poseven/functions/execv.hh"
#include "poseven/functions/open.hh"
#include "poseven/functions/stat.hh"
#include "poseven/functions/vfork.hh"
#include "poseven/functions/wait.hh"
#include "poseven/functions/write.hh"
#include "poseven/sequences/directory_contents.hh"

// pfiles
#include "pfiles/common.hh"

#if defined( __MACOS__ ) || defined( __APPLE__ )

// Divergence
#include "Divergence/Utilities.hh"

#else

typedef unsigned int OSType;

#endif

// Arcana
#include "HTTP.hh"

// Orion
#include "Orion/get_options.hh"
#include "Orion/Main.hh"


#define HTTP_VERSION  "HTTP/1.0"


namespace tool
{
	
	namespace p7 = poseven;
	namespace o = orion;
	
	
	using namespace io::path_descent_operators;
	
	
	static const char* gDocumentRoot = "/var/www";
	
	
	static char ToCGI( char c )
	{
		return c == '-' ? '_' : std::toupper( c );
	}
	
	static bool eq( const char* a, size_t a_size,
	                const char* b, size_t b_size )
	{
		return a_size == b_size  &&  std::equal( a, a + a_size, b );
	}
	
	static void SetCGIVariables( const HTTP::MessageReceiver& request )
	{
		const HTTP::HeaderIndex& index = request.GetHeaderIndex();
		
		const char* stream = request.GetHeaderStream();
		
		for ( HTTP::HeaderIndex::const_iterator it = index.begin();  it != index.end();  ++it )
		{
			const size_t prefix_length = STRLEN( "HTTP_" );
			
			const size_t name_length = it->colon_offset - it->field_offset;
			
			plus::string name_string;
			
			char* p = name_string.reset( prefix_length + name_length );
			
			char* name = p + prefix_length;
			
			std::transform( stream + it->field_offset,
			                stream + it->colon_offset,
			                name,
			                std::ptr_fun( ToCGI ) );
			
			const size_t len = name_length;
			
			const bool has_http =  !eq( name, len, STR_LEN( "CONTENT_LENGTH" ) )
			                    && !eq( name, len, STR_LEN( "CONTENT_TYPE"   ) );
			
			if ( has_http )
			{
				name = (char*) memcpy( p, STR_LEN( "HTTP_" ) );
			}
			
			plus::string value( stream + it->value_offset,
			                    stream + it->crlf_offset );
			
			setenv( name, value.c_str(), 1 );
		}
	}
	
	static void ForkExecWait( char const* const             argv[],
	                          const HTTP::MessageReceiver&  request )
	{
		const plus::string& partialData = request.GetPartialContent();
		
		bool partial_data_exist = !partialData.empty();
		
		int pipe_ends[2];
		
		if ( partial_data_exist )
		{
			pipe( pipe_ends );
		}
		
		p7::fd_t reader = p7::fd_t( pipe_ends[0] );
		p7::fd_t writer = p7::fd_t( pipe_ends[1] );
		
		p7::pid_t pid = POSEVEN_VFORK();
		
		if ( pid == 0 )
		{
			if ( partial_data_exist )
			{
				close( writer );
				
				dup2( reader, p7::stdin_fileno );  // read from pipe instead of socket
				
				close( reader );
			}
			
			// This eliminates LOCAL_EDITOR, PATH, and SECURITYSESSIONID.
			// We'll have to consider other approaches.
			//clearenv();
			
			SetCGIVariables( request );
			
			p7::execv( argv );
		}
		
		if ( partial_data_exist )
		{
			close( reader );
			
			p7::write( writer, partialData );
			
			p7::pump( p7::stdin_fileno, writer );
			
			close( writer );
		}
		
		p7::wait();
	}
	
	
	struct ParsedRequest
	{
		plus::string  method;
		plus::string  resource;
		plus::string  version;
	};
	
	class bad_http_request {};
	
	static ParsedRequest ParseRequest( const plus::string& request )
	{
		// E.g.  "GET / HTTP/1.0"
		
		const char* begin = request.data();
		const char* end   = begin + request.size();
		
		ParsedRequest parsed;
		
		// Find the first space (which ends the request method)
		const char* space = std::find( begin, end, ' ' );
		
		if ( space == end )
		{
			throw bad_http_request();
		}
		
		parsed.method.assign( request, 0, space - begin );  // e.g. "GET"
		
		// The resource starts after the space
		const char* resource = space + 1;
		
		// and ends with the next space
		space = std::find( resource, end, ' ' );
		
		if ( space == end )
		{
			throw bad_http_request();
		}
		
		parsed.resource.assign( request, resource - begin, space - resource );  // e.g. "/logo.png"
		
		// HTTP version string starts after the second space
		const char* version = space + 1;
		
		// and runs to the end
		parsed.version.assign( request, version - begin );  // e.g. "HTTP/1.1"
		
		return parsed;
	}
	
	class ResourceParser
	{
		private:
			plus::string resource;
		
		public:
			class const_iterator
			{
				private:
					const plus::string& s;
					plus::string::size_type pos, end;
				
				public:
					enum { npos = plus::string::npos };
					struct End  {};
					static End kEnd;
					
					const_iterator( const plus::string& s )
					:
						s  ( s                  ),
						pos( 1                  ),
						end( s.find( '/', pos ) )
					{}
					
					const_iterator( const plus::string& s, End )
					:
						s  ( s    ),
						pos( npos ),
						end( npos )
					{}
					
					plus::string operator*() const  { return s.substr( pos, end - pos ); }
					
					const_iterator& operator++()
					{
						if ( end + 1 < s.size() )
						{
							pos = end + 1;
							end = s.find( '/' , pos );
						}
						else
						{
							pos = end = plus::string::npos;
						}
						
						return *this;
					}
					
					const_iterator operator++( int )
					{
						const_iterator old = *this;
						++*this;
						return old;
					}
					
					friend bool operator==( const const_iterator& a, const const_iterator& b )
					{
						return a.pos == b.pos
							&& a.end == b.end;
					}
					
					friend bool operator!=( const const_iterator& a, const const_iterator& b )
					{
						return !( a == b );
					}
			};
			
			ResourceParser( const plus::string& resource ) : resource( resource + "/" )  {}
			
			const_iterator begin() const  { return const_iterator( resource                       ); }
			const_iterator end()   const  { return const_iterator( resource, const_iterator::kEnd ); }
	};
	
	static inline char check_xdigit( char c )
	{
		if ( !std::isxdigit( c ) )
		{
			p7::throw_errno( ENOENT );  // FIXME
		}
		
		return c;
	}
	
	static plus::string expand_percent_escapes( const plus::string& escaped )
	{
		plus::var_string result;
		
		//const char* p = &*escaped.begin();
		const char* end = &*escaped.end();
		
		for ( const char* p = &*escaped.begin();  p < end;  ++p )
		{
			char c = *p;
			
			if ( c == '%' )
			{
				char high = check_xdigit( *++p );
				char low  = check_xdigit( *++p );
				
				c = gear::decoded_hex_digit( high ) << 4
				  | gear::decoded_hex_digit( low  );
			}
			
			result += c;
		}
		
		return result;
	}
	
	static plus::string LocateResource( const plus::string& resource )
	{
		ResourceParser parser( resource );
		
		plus::string pathname = gDocumentRoot;
		
		// FIXME:  This can be an algorithm
		typedef ResourceParser::const_iterator const_iterator;
		for ( const_iterator it = parser.begin();  it != parser.end();  ++it )
		{
			plus::string component = expand_percent_escapes( *it );
			
			if ( !component.empty() && component[0] == '.' )
			{
				// No screwing around with the pathname, please.
				p7::throw_errno( ENOENT );
			}
			
			pathname = pathname / component;
		}
		
		p7::stat( pathname );  // Throw if nonexistent
		
		return pathname;
	}
	
	static plus::string FilenameExtension(const plus::string& filename)
	{
		// This will break for index.html.en and similar
		plus::string::size_type lastDot = filename.find_last_of( "." );
		
		if ( lastDot != plus::string::npos )
		{
			return filename.substr( lastDot, plus::string::npos );
		}
		else
		{
			return "";
		}
	}
	
	static const char* GuessContentType( const plus::string& filename, ::OSType type )
	{
		plus::string ext = FilenameExtension( filename );
		
		if ( ext == ".html" )
		{
			return "text/html";
		}
		else if ( ext == ".css" )
		{
			return "text/css";
		}
	#if TARGET_OS_MAC
		else if ( type == 'TEXT' )
		{
			return "text/plain";
		}
		else if ( type == 'BINA' )
		{
			return "application/x-macbinary";
		}
	#endif
		
		return "application/octet-stream";
	}
	
	static void DumpFile( const plus::string& pathname )
	{
		p7::pump( io::open_for_reading( pathname ), p7::stdout_fileno );
	}
	
	static void ListDir( const plus::string& pathname )
	{
		typedef io::directory_contents_traits< plus::string >::container_type directory_container;
		
		directory_container contents = io::directory_contents( pathname );
		
		typedef directory_container::const_iterator Iter;
		
		for ( Iter it = contents.begin();  it != contents.end();  ++it )
		{
			plus::var_string listing = *it;
			
			listing += "\n";
			
			p7::write( p7::stdout_fileno, listing );
		}
		
	}
	
	#define HTTP_ERROR( error )             \
		                                    \
		HTTP_VERSION " " error      "\r\n"  \
		"Content-Type: text/html"   "\r\n"  \
		                            "\r\n"  \
		"<title>" error "</title>"  "\r\n"  \
		"<p>"     error "</p>"      "\r\n"
	
	
	static void SendResponse( const HTTP::MessageReceiver& request )
	{
		plus::string status_line = request.GetStatusLine();
		
		p7::write( p7::stderr_fileno, STR_LEN( "  " ) );
		p7::write( p7::stderr_fileno, status_line );
		
		ParsedRequest parsed = ParseRequest( status_line );
		
		plus::string pathname;
		
		try
		{
			pathname = LocateResource( parsed.resource );
		}
		catch ( ... )
		{
			p7::write( p7::stdout_fileno,
			           STR_LEN( HTTP_ERROR( "404 Not Found" ) ) );
			
			return;
		}
		
		if ( strncmp( parsed.resource.c_str(), STR_LEN( "/cgi-bin/" ) ) == 0 )
		{
			const char* path = pathname.c_str();
			
			char const* const argv[] = { path, NULL };
			
			ForkExecWait( argv, request );
		}
		else
		{
			bool is_dir = false;
			
			if ( p7::s_isdir( p7::stat( pathname ) ) )
			{
				if ( *(pathname.end() - 1) != '/' )
				{
					p7::write( p7::stdout_fileno,
					           STR_LEN( HTTP_ERROR( "404 Not Found" ) ) );
					
					return;
				}
				
				plus::string index_html = pathname / "index.html";
				
				if ( io::file_exists( index_html ) )
				{
					pathname = index_html;
				}
				else
				{
					is_dir = true;
				}
			}
			
			OSType type = 0;
			
		#if TARGET_OS_MAC
			
			type = kUnknownType;
			
			FInfo info = { 0 };
			
			if ( !is_dir )
			{
				FSSpec file = Divergence::ResolvePathToFSSpec( pathname.c_str() );
				
				::OSErr err = FSpGetFInfo( &file, &info );
				
				if ( err == noErr )
				{
					type = info.fdType;
				}
			}
			
		#endif
			
			const char* contentType = is_dir ? "text/plain" : GuessContentType( pathname, type );
			
			plus::var_string responseHeader = HTTP_VERSION " 200 OK\r\n";
			
			responseHeader += HTTP::HeaderFieldLine( "Content-Type",  contentType                   );
			
		#if TARGET_OS_MAC
			
			responseHeader += HTTP::HeaderFieldLine( "X-Mac-Type",    plus::encode_32_bit_hex( info.fdType    ) );
			responseHeader += HTTP::HeaderFieldLine( "X-Mac-Creator", plus::encode_32_bit_hex( info.fdCreator ) );
			
		#endif
			
			responseHeader += "\r\n";
			
			p7::write( p7::stdout_fileno, responseHeader );
			
			if ( parsed.method != "HEAD" )
			{
				is_dir ? ListDir( pathname ) : DumpFile( pathname );
			}
		}
	}
	
	int Main( int argc, char** argv )
	{
		o::bind_option_to_variable( "--doc-root", gDocumentRoot );
		
		o::get_options( argc, argv );
		
		sockaddr_in peer;
		socklen_t peerlen = sizeof peer;
		
		if ( getpeername( 0, (sockaddr*)&peer, &peerlen ) == 0 )
		{
			std::fprintf( stderr, "%s:%d",
			                       inet_ntoa( peer.sin_addr ),
			                          peer.sin_port );
		}
		
		HTTP::MessageReceiver request;
		
		request.ReceiveHeader( p7::stdin_fileno );
		
		SendResponse( request );
		
		p7::write( p7::stderr_fileno, STR_LEN( "\n" ) );
		
		return 0;
	}

}


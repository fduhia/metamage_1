// FileDescriptor.hh
// -----------------
//
// Maintained by Joshua Juran

// Part of the Nitrogen project.
//
// Written 2006-2007 by Joshua Juran.
//
// This code was written entirely by the above contributor, who places it
// in the public domain.


#ifndef POSEVEN_FILEDESCRIPTOR_HH
#define POSEVEN_FILEDESCRIPTOR_HH

// Standard C
#include <errno.h>

// POSIX
#include <unistd.h>

// Nucleus
#include "Nucleus/ID.h"
#include "Nucleus/Owned.h"

// Io
#include "io/io.hh"

// POSeven
#include "POSeven/Errno.hh"


namespace poseven
{
	
	typedef Nucleus::ID< class fd_t_tag, int >::Type fd_t;
	
}

namespace Nucleus
{
	
	template <>
	struct Disposer< poseven::fd_t > : std::unary_function< poseven::fd_t, void >,
	                                   POSeven::DefaultDestructionErrnoPolicy
	{
		void operator()( poseven::fd_t fd ) const
		{
			// FIXME
			// HandleDestructionPOSIXError( ::close( fd ) );
			
			if ( ::close( fd ) == -1 )
			{
				HandleDestructionErrno( errno );
			}
		}
	};
	
}

namespace poseven
{
	
	namespace constants
	{
		
		const static fd_t stdin_fileno  = fd_t( STDIN_FILENO  );
		const static fd_t stdout_fileno = fd_t( STDOUT_FILENO );
		const static fd_t stderr_fileno = fd_t( STDERR_FILENO );
		
	}
	
	using namespace constants;
	
	inline void close( Nucleus::Owned< fd_t > fd )
	{
		throw_posix_result( ::close( fd.Release() ) );
	}
	
	inline ssize_t read( fd_t fd, char* buffer, std::size_t bytes_requested )
	{
		return throw_posix_result( ::read( fd, buffer, bytes_requested ) );
	}
	
	inline ssize_t write( fd_t fd, const char* buffer, std::size_t bytes_requested )
	{
		return throw_posix_result( ::write( fd, buffer, bytes_requested ) );
	}
	
}

struct stat;

namespace poseven
{
	
	struct posix_io_details
	{
		typedef std::string file_spec;
		typedef std::string optimized_directory_spec;
		
		typedef const std::string& filename_parameter;
		typedef       std::string  filename_result;
		
		typedef struct ::stat file_catalog_record;
		
		typedef fd_t stream;
		
		typedef std::size_t byte_count;
		
		typedef off_t position_offset;
	};
	
}

namespace io
{
	
	template <> struct filespec_traits< std::string > : public poseven::posix_io_details {};
	
	template < class ByteCount >
	inline ssize_t read( poseven::fd_t fd, char* buffer, ByteCount byteCount, overload = overload() )
	{
		return poseven::read( fd, buffer, Nucleus::Convert< std::size_t >( byteCount ) );
	}
	
	template < class ByteCount >
	inline ssize_t write( poseven::fd_t fd, const char* buffer, ByteCount byteCount, overload = overload() )
	{
		return poseven::write( fd, buffer, Nucleus::Convert< std::size_t >( byteCount ) );
	}
	
}

#endif


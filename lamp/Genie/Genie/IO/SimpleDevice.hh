/*	===============
 *	SimpleDevice.hh
 *	===============
 */

#ifndef GENIE_IO_SIMPLEDEVICE_HH
#define GENIE_IO_SIMPLEDEVICE_HH

// POSIX
#include <fcntl.h>

 // Genie
#include "Genie/IO/Device.hh"


namespace Genie
{
	
	typedef ssize_t (*Reader)( char      *, std::size_t );
	typedef ssize_t (*Writer)( char const*, std::size_t );
	
	struct DeviceIOSpec
	{
		const char* name;
		Reader reader;
		Writer writer;
	};
	
	class SimpleDeviceHandle : public DeviceHandle
	{
		private:
			const DeviceIOSpec& io;
		
		public:
			SimpleDeviceHandle( const DeviceIOSpec& io ) : DeviceHandle( O_RDWR ), io( io )
			{
			}
			
			FSTreePtr GetFile();
			
			unsigned int SysPoll()  { return kPollRead | kPollWrite; }
			
			ssize_t SysRead( char* data, std::size_t byteCount );
			
			ssize_t SysWrite( const char* data, std::size_t byteCount );
			
			memory_mapping_ptr Map( size_t length, int prot, int flags, off_t offset );
	};
	
}

#endif


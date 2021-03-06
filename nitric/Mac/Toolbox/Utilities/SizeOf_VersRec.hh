/*
	Mac/Toolbox/Utilities/SizeOf_VersRec.hh
	---------------------------------------
*/

#ifndef MAC_TOOLBOX_UTILITIES_SIZEOFVERSREC_HH
#define MAC_TOOLBOX_UTILITIES_SIZEOFVERSREC_HH

// Standard C/C++
#include <cstddef>

// Mac OS X
#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif

// Mac OS
#ifndef __MACTYPES__
#include <MacTypes.h>
#endif

// nucleus
#ifndef NUCLEUS_ENUMERATIONTRAITS_HH
#include "nucleus/enumeration_traits.hh"
#endif


namespace Mac
{
	
	inline std::size_t SizeOf_VersRec( const VersRec& version )
	{
		const UInt8 shortVersionLength = version.shortVersion[ 0 ];
		
		// The long version string immediately follows the short version string.
		
		const UInt8* longVersion = version.shortVersion + 1 + shortVersionLength;
		
		const UInt8 longVersionLength = longVersion[ 0 ];
		
		return sizeof (::NumVersion)
		     + sizeof (SInt16)
		     + 1 + shortVersionLength
		     + 1 + longVersionLength;
	}
	
}

#endif


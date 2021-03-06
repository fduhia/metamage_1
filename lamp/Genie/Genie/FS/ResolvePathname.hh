/*	==================
 *	ResolvePathname.hh
 *	==================
 */

#ifndef GENIE_FILESYSTEM_RESOLVEPATHNAME_HH
#define GENIE_FILESYSTEM_RESOLVEPATHNAME_HH

// plus
#include "plus/string.hh"

// Genie
#include "Genie/FS/FSTree.hh"


namespace Genie
{
	
	bool ResolveLinks_InPlace( FSTreePtr& link );
	
	FSTreePtr ResolveRelativePath( const char*    begin,
	                               std::size_t    length,
	                               const FSTree*  current );
	
	
	FSTreePtr ResolveAbsolutePath( const char*  begin,
	                               std::size_t  length );
	
	FSTreePtr ResolveAbsolutePath( const plus::string& path );
	
	
	FSTreePtr ResolvePathname( const char*    begin,
	                           std::size_t    length,
	                           const FSTree*  current = NULL );
	
	FSTreePtr ResolvePathname( const char*    pathname,
	                           const FSTree*  current = NULL );
	
	FSTreePtr ResolvePathname( const plus::string&  pathname,
	                           const FSTree*        current = NULL );
	
}

#endif


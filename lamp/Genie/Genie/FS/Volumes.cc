/*
	Volumes.cc
	----------
*/

#include "Genie/FS/Volumes.hh"

// Standard C++
#include <algorithm>

// Mac OS
#ifndef __FILES__
#include <Files.h>
#endif

// plus
#include "plus/var_string.hh"

// Nitrogen
#include "Mac/Files/Types/FSDirSpec.hh"
#include "Mac/Files/Types/FSVolumeRefNum.hh"

#include "Nitrogen/OSStatus.hh"
#include "Nitrogen/Str.hh"

// Genie
#include "Genie/FS/FSSpec.hh"
#include "Genie/FS/FSTreeCache.hh"
#include "Genie/FS/FSTree_Directory.hh"
#include "Genie/FS/ResolvableSymLink.hh"


namespace Genie
{
	
	namespace n = nucleus;
	namespace N = Nitrogen;
	
	
	static plus::string MacFromUnixName( const plus::string& name )
	{
		plus::var_string result;
		
		result.resize( name.size() );
		
		std::replace_copy( name.begin(),
		                   name.end(),
		                   result.begin(),
		                   ':',
		                   '/' );
		
		return result;
	}
	
	static plus::string UnixFromMacName( ConstStr255Param name )
	{
		plus::var_string result;
		
		result.resize( name[0] );
		
		std::replace_copy( &name[ 1           ],
		                   &name[ 1 + name[0] ],
		                   result.begin(),
		                   '/',
		                   ':' );
		
		return result;
	}
	
	
	static Mac::FSVolumeRefNum GetVRefNum( const plus::string& name )
	{
		N::Str255 name_copy = name;
		
		HParamBlockRec pb;
		
		pb.volumeParam.ioVRefNum  = 0;
		pb.volumeParam.ioNamePtr  = name_copy;
		pb.volumeParam.ioVolIndex = -1;  // use use ioNamePtr/ioVRefNum combination
		
		N::ThrowOSStatus( ::PBHGetVInfoSync( &pb ) );
		
		return Mac::FSVolumeRefNum( pb.volumeParam.ioVRefNum );
	}
	
	
	class FSTree_Volumes : public FSTree_Directory
	{
		public:
			FSTree_Volumes( const FSTreePtr&     parent,
			                const plus::string&  name )
			:
				FSTree_Directory( parent, name )
			{
			}
			
			ino_t Inode() const  { return fsRtParID; }
			
			FSTreePtr Lookup_Child( const plus::string& name, const FSTree* parent ) const;
			
			void IterateIntoCache( FSTreeCache& cache ) const;
	};
	
	
	FSTreePtr New_FSTree_Volumes( const FSTreePtr&     parent,
	                              const plus::string&  name,
	                              const void*          args )
	{
		return seize_ptr( new FSTree_Volumes( parent, name ) );
	}
	
	
	class FSTree_Volumes_Link : public FSTree_ResolvableSymLink
	{
		public:
			FSTree_Volumes_Link( const FSTreePtr&     parent,
			                     const plus::string&  name )
			:
				FSTree_ResolvableSymLink( parent, name )
			{
			}
			
			FSTreePtr ResolveLink() const;
	};
	
	FSTreePtr FSTree_Volumes_Link::ResolveLink() const
	{
		// Convert ':' to '/'
		plus::var_string mac_name = MacFromUnixName( Name() );
		
		mac_name += ":";
		
		const Mac::FSVolumeRefNum vRefNum = GetVRefNum( mac_name );
		
		const Mac::FSDirSpec dir = n::make< Mac::FSDirSpec >( vRefNum, Mac::fsRtDirID );
		
		return FSTreeFromFSDirSpec( dir, VolumeIsOnServer( vRefNum ) );
	}
	
	
	FSTreePtr FSTree_Volumes::Lookup_Child( const plus::string& name, const FSTree* parent ) const
	{
		return seize_ptr( new FSTree_Volumes_Link( (parent ? parent : this)->Self(), name ) );
	}
	
	void FSTree_Volumes::IterateIntoCache( FSTreeCache& cache ) const
	{
		for ( int i = 1;  true;  ++i )
		{
			Str27 mac_name = "\p";
			
			HParamBlockRec pb;
			
			pb.volumeParam.ioNamePtr  = mac_name;
			pb.volumeParam.ioVRefNum  = 0;
			pb.volumeParam.ioVolIndex = i;
			
			const OSErr err = ::PBHGetVInfoSync( &pb );
			
			if ( err == nsvErr )
			{
				break;
			}
			
			N::ThrowOSStatus( err );
			
			const ino_t inode = -pb.volumeParam.ioVRefNum;
			
			const plus::string name = UnixFromMacName( mac_name );
			
			cache.push_back( FSNode( inode, name ) );
		}
	}
	
}

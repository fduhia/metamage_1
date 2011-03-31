/*	=========
 *	FSTree.cc
 *	=========
 */

#include "Genie/FS/FSTree.hh"

// POSIX
#include "fcntl.h"
#include "sys/stat.h"

// plus
#include "plus/hexidecimal.hh"
#include "plus/string/concat.hh"

// Debug
#include "debug/assert.hh"

// Nitrogen
#include "Nitrogen/DateTimeUtils.hh"
#include "Nitrogen/OSUtils.hh"
#include "Nitrogen/Threads.hh"

// poseven
#include "poseven/types/errno_t.hh"

// TimeOff
#include "TimeOff/TimeOff.hh"

// Genie
#include "Genie/FS/FSTreeCache_Impl.hh"
#include "Genie/IO/VirtualDirectory.hh"


namespace Genie
{
	
	namespace N = Nitrogen;
	namespace p7 = poseven;
	
	
	const FSTreePtr null_FSTreePtr = FSTreePtr();
	
	
	typedef boost::shared_ptr< const FSTreeCache > FSTreeCachePtr;
	
	class FSIterator_Cache : public FSIterator
	{
		private:
			FSTreeCachePtr  contents;
			std::size_t     nextOffset;
		
		public:
			FSIterator_Cache( const FSTreeCachePtr& cache ) : contents( cache ), nextOffset( 0 )  {}
			
			FSNode Get() const
			{
				const FSTreeCache_Impl* impl = static_cast< const FSTreeCache_Impl* >( contents.get() );
				
				return nextOffset < impl->size() ? impl->at( nextOffset )
				                                 : FSNode();
			}
			
			void Advance()  { ++nextOffset; }
			
			void Rewind()  { nextOffset = 0; }
			
			void Seek( off_t index )  { nextOffset = index; }
			
			off_t Tell() const  { return nextOffset; }
	};
	
	
	FSNode::~FSNode()
	{
	}
	
	FSIterator::~FSIterator()
	{
	}
	
	FSTree::FSTree() : itsParent(), itsName(), itsMode()
	{
	}
	
	static inline plus::string NameFromPtr( const FSTree* ptr )
	{
		return plus::encode_32_bit_hex( (unsigned) ptr );
	}
	
	FSTree::FSTree( const FSTreePtr&     parent,
	                const plus::string&  name )
	:
		itsParent( parent ),
		itsName  ( name[0] == '/' ? NameFromPtr( this )
		                          : name ),
		itsMode  ( S_IFREG | S_IRUSR )  // reasonable default
	{
	}
	
	FSTree::FSTree( const FSTreePtr&     parent,
	                const plus::string&  name,
	                mode_t               mode )
	:
		itsParent( parent ),
		itsName  ( name[0] == '/' ? NameFromPtr( this )
		                          : name ),
		itsMode  ( mode )
	{
	}
	
	FSTree::~FSTree()
	{
	}
	
	plus::string FSTree::Pathname() const
	{
		plus::string pathname = Name();
		
		FSTreePtr tree = Parent();
		
		if ( tree.get() == this  &&  !pathname.empty() )
		{
			// Anonymous files have null parents (but so does root)
			return pathname;
		}
		
		while ( true )
		{
			const plus::string& name = tree->Name();
			
			pathname = name + "/" + pathname;
			
			// Root dir has empty name
			
			if ( name.empty() )
			{
				break;
			}
			
			tree = tree->Parent();
		}
		
		return pathname;
	}
	
	
	bool FSTree::Exists() const
	{
		return itsMode != 0;
	}
	
	bool FSTree::IsFile() const
	{
		// A file as used here is any existing non-directory.
		return itsMode != 0  &&  !S_ISDIR( itsMode );
	}
	
	bool FSTree::IsDirectory() const
	{
		return S_ISDIR( itsMode );
	}
	
	bool FSTree::IsLink() const
	{
		return S_ISLNK( itsMode );
	}
	
	bool FSTree::IsPipe() const
	{
		return S_ISFIFO( itsMode );
	}
	
	FSTreePtr FSTree::Parent() const
	{
		return itsParent.get() ? itsParent : Self();
	}
	
	ino_t FSTree::Inode() const
	{
		return 0;
	}
	
	ino_t FSTree::ParentInode() const
	{
		return Parent()->Inode();
	}
	
	mode_t FSTree::FilePermMode() const
	{
		mode_t perm = IsDirectory() ? S_IRUSR | S_IWUSR | S_IXUSR
		            : IsLink()      ? S_IRUSR | S_IWUSR | S_IXUSR
		            :                 itsMode & 07777;
		
		return perm;
	}
	
	void FSTree::Stat( struct ::stat& sb ) const
	{
		if ( ! Exists() )
		{
			p7::throw_errno( ENOENT );
		}
		
		const unsigned long timeDiff = TimeOff::MacToUnixTimeDifference();
		
		time_t now = N::GetDateTime() - timeDiff;
		
		const mode_t type = IsDirectory() ? S_IFDIR
		                  : IsLink()      ? S_IFLNK
		                  :                 itsMode & S_IFMT;
		
		sb.st_ino = Inode();
		
		sb.st_mode = type | FilePermMode();
		
		sb.st_nlink = 1;
		
		// fstatat() or fstat() will fill in sb.st_size
		
		sb.st_blksize = 4096;
		
		sb.st_atime = now;
		sb.st_mtime = now;
		sb.st_ctime = now;
	}
	
	void FSTree::ChangeMode( mode_t /*mode*/ ) const
	{
		p7::throw_errno( EPERM );
	}
	
	void FSTree::SetTimes() const
	{
		p7::throw_errno( EPERM );
	}
	
	void FSTree::SetTimes( const struct timespec times[2] ) const
	{
		p7::throw_errno( EPERM );
	}
	
	void FSTree::Delete() const
	{
		p7::throw_errno( EPERM );
	}
	
	void FSTree::Rename( const FSTreePtr& destination ) const
	{
		p7::throw_errno( EINVAL );
	}
	
	void FSTree::HardLink( const FSTreePtr& destination ) const
	{
		p7::throw_errno( EINVAL );
	}
	
	void FSTree::CopyFile( const FSTreePtr& destination ) const
	{
		p7::throw_errno( EINVAL );
	}
	
	off_t FSTree::GetEOF() const
	{
		// Errors are meaningless here since there's no POSIX call specifically
		// to get EOF (as there is truncate() for setting EOF).  We only care so
		// we can populate stat::st_size.  Just return zero.
		
		return 0;
	}
	
	void FSTree::SetEOF( off_t /*length*/ ) const
	{
		// This confuses MWCPPC when optimizing:
		//p7::throw_errno( IsDirectory() ? EISDIR : EINVAL );
		// internal compiler error: File: 'PCodeUtilities.c' Line: 80
		
		if ( IsDirectory() )
		{
			p7::throw_errno( EISDIR );
		}
		else
		{
			p7::throw_errno( EINVAL );
		}
	}
	
	plus::string FSTree::ReadLink() const
	{
		throw p7::errno_t( EINVAL );
	}
	
	FSTreePtr FSTree::ResolveLink() const
	{
		return Self();
	}
	
	void FSTree::SymLink( const plus::string& target ) const
	{
		p7::throw_errno( EINVAL );
	}
	
	boost::shared_ptr< IOHandle > FSTree::Open( OpenFlags flags, mode_t mode ) const
	{
		bool creating  = flags & O_CREAT;
		bool excluding = flags & O_EXCL;
		
		if ( creating && excluding )
		{
			p7::throw_errno( EPERM );
		}
		
		return Open( flags );
	}
	
	boost::shared_ptr< IOHandle > FSTree::Open( OpenFlags /*flags*/ ) const
	{
		throw p7::errno_t( ENOENT );  // Assume read attempt if no mode
	}
	
	shared_exec_handle FSTree::GetExecutable() const
	{
		throw p7::errno_t( ENOEXEC );
	}
	
	
	boost::shared_ptr< IOHandle > FSTree::OpenDirectory() const
	{
		if ( !IsDirectory() )
		{
			p7::throw_errno( Exists() ? ENOTDIR : ENOENT );
		}
		
		return seize_ptr( new VirtualDirHandle( Self() ) );
	}
	
	boost::shared_ptr< IOHandle > FSTree::ChangeToDirectory() const
	{
		return OpenDirectory();
	}
	
	void FSTree::CreateDirectory( mode_t /*mode*/ ) const
	{
		p7::throw_errno( EPERM );
	}
	
	FSTreePtr FSTree::Lookup( const plus::string& name, const FSTree* parent ) const
	{
		if ( name == "." )
		{
			return Self();
		}
		else if ( name == ".." )
		{
			return Parent();
		}
		
		return Lookup_Child( name, parent );
	}
	
	FSTreePtr FSTree::Lookup_Child( const plus::string& name, const FSTree* parent ) const
	{
		throw p7::errno_t( ENOTDIR );
	}
	
	FSIteratorPtr FSTree::Iterate() const
	{
		FSTreeCache_Impl cache;
		
		cache.push_back( FSNode( Inode(),       "."  ) );
		cache.push_back( FSNode( ParentInode(), ".." ) );
		
		IterateIntoCache( cache );
		
		FSTreeCache_Impl* newCache = new FSTreeCache_Impl();
		
		FSTreeCachePtr cachePtr( newCache );
		
		swap( cache, *newCache );
		
		return FSIteratorPtr( new FSIterator_Cache( cachePtr ) );
	}
	
	void FSTree::IterateIntoCache( FSTreeCache& cache ) const
	{
		throw p7::errno_t( ENOTDIR );
	}
	
	FSTreePtr FSTree::ResolvePath( const char*& begin, const char* end ) const
	{
		ASSERT( begin < end );
		
		ASSERT( begin[0] != '/' );
		
		const char* slash = std::find( begin, end, '/' );
		
		plus::string name( begin, slash );
		
		begin = slash;
		
		return Lookup( name );
	}
	
	void FSTree::Attach( const FSTreePtr& target ) const
	{
		p7::throw_errno( EINVAL );
	}
	
}


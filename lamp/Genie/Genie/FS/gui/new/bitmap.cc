/*
	gui/new/bitmap.cc
	-----------------
*/

#include "Genie/FS/gui/new/bitmap.hh"

// POSIX
#include <sys/stat.h>

// Standard C++
#include <algorithm>

// plus
#include "plus/serialize.hh"

// nucleus
#include "nucleus/shared.hh"

// poseven
#include "poseven/types/errno_t.hh"

// Nitrogen
#include "Nitrogen/MacMemory.hh"
#include "Nitrogen/Quickdraw.hh"

// Pedestal
#include "Pedestal/View.hh"

// Genie
#include "Genie/FS/FSTree_Property.hh"
#include "Genie/FS/serialize_qd.hh"
#include "Genie/FS/Views.hh"
#include "Genie/FS/data_method_set.hh"
#include "Genie/FS/node_method_set.hh"
#include "Genie/IO/Handle.hh"
#include "Genie/Utilities/simple_map.hh"


namespace Genie
{
	
	namespace n = nucleus;
	namespace N = Nitrogen;
	namespace p7 = poseven;
	namespace Ped = Pedestal;
	
	
	struct BitMap_Parameters
	{
		n::shared< N::Ptr >  bits;
		
		BitMap               bitmap;
	};
	
	typedef simple_map< const FSTree*, BitMap_Parameters > BitMapMap;
	
	static BitMapMap gBitMapMap;
	
	
	static unsigned BitMap_n_bytes( const BitMap& bits )
	{
		const short n_rows = bits.bounds.bottom - bits.bounds.top;
		
		const short rowBytes = bits.rowBytes & 0x3FFF;
		
		return n_rows * rowBytes;
	}
	
	static off_t Bits_GetEOF( const FSTree* key )
	{
		return BitMap_n_bytes( gBitMapMap[ key ].bitmap );
	}
	
	class Bits_IO : public VirtualFileHandle< RegularFileHandle >
	{
		private:
			// non-copyable
			Bits_IO           ( const Bits_IO& );
			Bits_IO& operator=( const Bits_IO& );
		
		public:
			Bits_IO( const FSTreePtr& file, int flags )
			:
				VirtualFileHandle< RegularFileHandle >( file, flags )
			{
			}
			
			const FSTree* ViewKey();
			
			IOPtr Clone();
			
			ssize_t Positioned_Read( char* buffer, size_t n_bytes, off_t offset );
			
			ssize_t Positioned_Write( const char* buffer, size_t n_bytes, off_t offset );
			
			off_t GetEOF()  { return Bits_GetEOF( ViewKey() ); }
			
			//void Synchronize( bool metadata );
	};
	
	const FSTree* Bits_IO::ViewKey()
	{
		return GetFile()->owner();
	}
	
	IOPtr Bits_IO::Clone()
	{
		return new Bits_IO( GetFile(), GetFlags() );
	}
	
	ssize_t Bits_IO::Positioned_Read( char* buffer, size_t n_bytes, off_t offset )
	{
		const FSTree* view = ViewKey();
		
		BitMap_Parameters& params = gBitMapMap[ view ];
		
		const size_t pix_size = BitMap_n_bytes( params.bitmap );
		
		if ( offset >= pix_size )
		{
			return 0;
		}
		
		n_bytes = std::min< size_t >( n_bytes, pix_size - offset );
		
		const char* baseAddr = params.bitmap.baseAddr;
		
		memcpy( buffer, &baseAddr[ offset ], n_bytes );
		
		return n_bytes;
	}
	
	ssize_t Bits_IO::Positioned_Write( const char* buffer, size_t n_bytes, off_t offset )
	{
		const FSTree* view = ViewKey();
		
		BitMap_Parameters& params = gBitMapMap[ view ];
		
		if ( params.bitmap.baseAddr == NULL )
		{
			p7::throw_errno( EIO );
		}
		
		const size_t pix_size = BitMap_n_bytes( params.bitmap );
		
		if ( offset + n_bytes > pix_size )
		{
			if ( offset >= pix_size )
			{
				p7::throw_errno( EFAULT );
			}
			
			n_bytes = pix_size - offset;
		}
		
		char* baseAddr = params.bitmap.baseAddr;
		
		memcpy( &baseAddr[ offset ], buffer, n_bytes );
		
		InvalidateWindowForView( view );
		
		return n_bytes;
	}
	
	
	static bool has_bits( const FSTree* view )
	{
		return gBitMapMap[ view ].bits.get() != NULL;
	}
	
	static off_t bitmap_bits_geteof( const FSTree* node )
	{
		return Bits_GetEOF( node->owner() );
	}
	
	static IOPtr bitmap_bits_open( const FSTree* node, int flags, mode_t mode )
	{
		return new Bits_IO( node, flags );
	}
	
	static const data_method_set bitmap_bits_data_methods =
	{
		&bitmap_bits_open,
		&bitmap_bits_geteof
	};
	
	static const node_method_set bitmap_bits_methods =
	{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		&bitmap_bits_data_methods
	};
	
	
	class BitMapView : public Ped::View
	{
		private:
			typedef const FSTree* Key;
			
			Key itsKey;
		
		public:
			BitMapView( Key key ) : itsKey( key )
			{
			}
			
			void Draw( const Rect& bounds, bool erasing );
	};
	
	static inline short compute_rowBytes_from_bounds( const Rect& bounds )
	{
		return (bounds.right - bounds.left) + 15 >> 3 & ~1;
	}
	
	void BitMapView::Draw( const Rect& bounds, bool erasing )
	{
		const N::TransferMode mode = N::srcCopy;
		
		if ( erasing  &&  mode != N::srcCopy )
		{
			N::EraseRect( bounds );
		}
		
		BitMap_Parameters& params = gBitMapMap[ itsKey ];
		
		if ( !params.bitmap.rowBytes )
		{
			params.bitmap.bounds = bounds;
			
			params.bitmap.rowBytes = compute_rowBytes_from_bounds( bounds );
		}
		
		if ( params.bitmap.baseAddr == NULL )
		{
			size_t n_bytes = params.bitmap.rowBytes * (bounds.bottom - bounds.top);
			
			params.bits = N::NewPtrClear( n_bytes );
			
			params.bitmap.baseAddr = params.bits.get();
		}
		
		// Copy to dest
		N::CopyBits( &params.bitmap,
		             N::GetPortBitMapForCopyBits( N::GetQDGlobalsThePort() ),
		             params.bitmap.bounds,
		             bounds,
		             mode );
	}
	
	
	static boost::intrusive_ptr< Ped::View > CreateView( const FSTree* delegate )
	{
		BitMap_Parameters& params = gBitMapMap[ delegate ];
		
		params.bitmap.baseAddr = 0;
		params.bitmap.rowBytes = 0;
		
		params.bitmap.bounds.top    = 0;
		params.bitmap.bounds.left   = 0;
		params.bitmap.bounds.bottom = 0;
		params.bitmap.bounds.right  = 0;
		
		return new BitMapView( delegate );
	}
	
	
	static void DestroyDelegate( const FSTree* delegate )
	{
		gBitMapMap.erase( delegate );
	}
	
	
	struct BitMap_rowBytes : plus::serialize_unsigned< short >
	{
		static const bool is_mutable = false;
		
		static short Get( const BitMap& bits )
		{
			return bits.rowBytes & 0x3FFF;
		}
		
		static void Set( BitMap_Parameters& params, short depth );
	};
	
	struct BitMap_bounds : serialize_Rect
	{
		static const bool is_mutable = true;
		
		static Rect Get( const BitMap& bits )
		{
			return bits.bounds;
		}
		
		static void Set( BitMap_Parameters& params, const Rect& bounds );
	};
	
	void BitMap_bounds::Set( BitMap_Parameters& params, const Rect& bounds )
	{
		if ( ::EmptyRect( &bounds ) )
		{
			p7::throw_errno( EINVAL );
		}
		
		const short rowBytes = compute_rowBytes_from_bounds( bounds );
		
		if ( params.bitmap.baseAddr )
		{
			const size_t old_size = N::GetPtrSize( params.bitmap.baseAddr );
			
			params.bits.reset();
			
			const size_t new_size = rowBytes * (bounds.bottom - bounds.top);
			
			params.bitmap.baseAddr = ::NewPtrClear( new_size );
			
			if ( params.bitmap.baseAddr == NULL )
			{
				/*
					Try to reallocate old block.  This shouldn't fail, but
					theoretically it might throw memFullErr.
				*/
				params.bits = N::NewPtrClear( old_size );
				
				params.bitmap.baseAddr = params.bits.get();
				
				// Even if we succeed here, it's still a failure mode.
				p7::throw_errno( ENOMEM );
			}
			
			// Seize the newly allocated block
			params.bits = n::owned< N::Ptr >::seize( params.bitmap.baseAddr );
		}
		
		params.bitmap.bounds   = bounds;
		params.bitmap.rowBytes = rowBytes;
	}
	
	static const BitMap& get_bitmap( const FSTree* that )
	{
		BitMap_Parameters* it = gBitMapMap.find( that );
		
		if ( it == NULL )
		{
			p7::throw_errno( ENOENT );
		}
		
		return it->bitmap;
	}
	
	template < class Accessor >
	struct BitMap_Property : readwrite_property
	{
		static const std::size_t fixed_size = Accessor::fixed_size;
		
		static const bool can_set = Accessor::is_mutable;
		
		typedef typename Accessor::result_type result_type;
		
		static void get( plus::var_string& result, const FSTree* that, bool binary )
		{
			const BitMap& bits = get_bitmap( that );
			
			const result_type data = Accessor::Get( bits );
			
			Accessor::deconstruct::apply( result, data, binary );
		}
		
		static void set( const FSTree* that, const char* begin, const char* end, bool binary )
		{
			BitMap_Parameters& params = gBitMapMap[ that ];
			
			const result_type data = Accessor::reconstruct::apply( begin, end, binary );
			
			Accessor::Set( params, data );
			
			InvalidateWindowForView( that );
		}
	};
	
	
	static FSTreePtr bitmap_bits_Factory( const FSTree*        parent,
	                                      const plus::string&  name,
	                                      const void*          args )
	{
		const mode_t mode = has_bits( parent ) ? S_IFREG | 0600 : 0;
		
		return new FSTree( parent, name, mode, &bitmap_bits_methods );
	}
	
	#define PROPERTY( prop )  &new_property, &property_params_factory< BitMap_Property< prop > >::value
	
	static const vfs::fixed_mapping local_mappings[] =
	{
		{ "rowBytes", PROPERTY( BitMap_rowBytes ) },
		
		{ "bounds", PROPERTY( BitMap_bounds ) },
		
		{ "bits", &bitmap_bits_Factory },
		
		{ NULL, NULL }
	};
	
	FSTreePtr New_FSTree_new_bitmap( const FSTree*        parent,
	                                 const plus::string&  name,
	                                 const void* )
	{
		return New_new_view( parent,
		                     name,
		                     &CreateView,
		                     local_mappings,
		                     &DestroyDelegate );
	}
	
}


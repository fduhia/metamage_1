/*
	gui/new/caption.cc
	------------------
*/

#include "Genie/FS/gui/new/caption.hh"

// Standard C++
#include <algorithm>

// POSIX
#include <fcntl.h>
#include <sys/stat.h>

// plus
#include "plus/mac_utf8.hh"
#include "plus/serialize.hh"
#include "plus/var_string.hh"

// Pedestal
#include "Pedestal/Caption.hh"

// Genie
#include "Genie/FS/FSTree.hh"
#include "Genie/FS/FSTree_Property.hh"
#include "Genie/FS/Views.hh"
#include "Genie/FS/data_method_set.hh"
#include "Genie/FS/node_method_set.hh"
#include "Genie/IO/RegularFile.hh"
#include "Genie/IO/VirtualFile.hh"
#include "Genie/Utilities/simple_map.hh"


namespace Genie
{
	
	namespace Ped = Pedestal;
	
	
	struct CaptionParameters
	{
		plus::var_string  its_mac_text;
		plus::var_string  its_utf8_text;
		bool              itIsWrapped;
		bool              disabling;
		
		CaptionParameters() : itIsWrapped( true ), disabling()
		{
		}
	};
	
	typedef simple_map< const FSTree*, CaptionParameters > CaptionParametersMap;
	
	static CaptionParametersMap gCaptionParametersMap;
	
	
	class Caption : public Ped::Caption
	{
		private:
			typedef const FSTree* Key;
			
			Key itsKey;
			bool  itIsActive;
		
		public:
			Caption( Key key ) : itsKey( key ), itIsActive( true )
			{
			}
			
			plus::string Text() const;
			
			bool Wrapped() const;
			bool Disabled() const;
			
			void Activate( bool activating )  { itIsActive = activating; }
	};
	
	plus::string Caption::Text() const
	{
		plus::var_string result;
		
		if ( CaptionParameters* it = gCaptionParametersMap.find( itsKey ) )
		{
			result = it->its_mac_text;
		}
		
		std::replace( result.begin(), result.end(), '\n', '\r' );
		
		return result;
	}
	
	bool Caption::Wrapped() const
	{
		if ( CaptionParameters* it = gCaptionParametersMap.find( itsKey ) )
		{
			return it->itIsWrapped;
		}
		
		return true;
	}
	
	bool Caption::Disabled() const
	{
		if ( CaptionParameters* it = gCaptionParametersMap.find( itsKey ) )
		{
			return !itIsActive && it->disabling;
		}
		
		return false;
	}
	
	static boost::intrusive_ptr< Ped::View > CreateView( const FSTree* delegate )
	{
		return new Caption( delegate );
	}
	
	
	static void DestroyDelegate( const FSTree* delegate )
	{
		gCaptionParametersMap.erase( delegate );
	}
	
	
	static void CaptionText_SetEOF( const FSTree* text, off_t length )
	{
		const FSTree* view = text->owner();
		
		CaptionParameters& params = gCaptionParametersMap[ view ];
		
		params.its_utf8_text.resize( length );
		
		params.its_mac_text = plus::mac_from_utf8( params.its_utf8_text );
		
		InvalidateWindowForView( view );
	}
	
	class CaptionTextFileHandle : public VirtualFileHandle< RegularFileHandle >
	{
		public:
			CaptionTextFileHandle( const FSTreePtr& file, int flags )
			:
				VirtualFileHandle< RegularFileHandle >( file, flags )
			{
			}
			
			IOPtr Clone();
			
			const FSTree* ViewKey();
			
			plus::var_string& String()  { return gCaptionParametersMap[ ViewKey() ].its_utf8_text; }
			
			ssize_t Positioned_Read( char* buffer, size_t n_bytes, off_t offset );
			
			ssize_t Positioned_Write( const char* buffer, size_t n_bytes, off_t offset );
			
			off_t GetEOF()  { return String().size(); }
			
			void SetEOF( off_t length )  { CaptionText_SetEOF( GetFile().get(), length ); }
	};
	
	IOPtr CaptionTextFileHandle::Clone()
	{
		return new CaptionTextFileHandle( GetFile(), GetFlags() );
	}
	
	const FSTree* CaptionTextFileHandle::ViewKey()
	{
		return GetFile()->owner();
	}
	
	ssize_t CaptionTextFileHandle::Positioned_Read( char* buffer, size_t n_bytes, off_t offset )
	{
		const plus::string& s = String();
		
		if ( offset >= s.size() )
		{
			return 0;
		}
		
		n_bytes = std::min< size_t >( n_bytes, s.size() - offset );
		
		memcpy( buffer, &s[ offset ], n_bytes );
		
		return n_bytes;
	}
	
	ssize_t CaptionTextFileHandle::Positioned_Write( const char* buffer, size_t n_bytes, off_t offset )
	{
		const FSTree* view = ViewKey();
		
		CaptionParameters& params = gCaptionParametersMap[ view ];
		
		plus::var_string& s = params.its_utf8_text;
		
		if ( offset + n_bytes > s.size() )
		{
			s.resize( offset + n_bytes );
		}
		
		std::copy( buffer,
		           buffer + n_bytes,
		           s.begin() + offset );
		
		params.its_mac_text = plus::mac_from_utf8( s );
		
		InvalidateWindowForView( view );
		
		return n_bytes;
	}
	
	
	static void caption_text_seteof( const FSTree* node, off_t length )
	{
		CaptionText_SetEOF( node, length );
	}
	
	static off_t caption_text_geteof( const FSTree* node )
	{
		return gCaptionParametersMap[ node->owner() ].its_utf8_text.size();
	}
	
	static IOPtr caption_text_open( const FSTree* node, int flags, mode_t mode )
	{
		return new CaptionTextFileHandle( node, flags );
	}
	
	static const data_method_set caption_text_data_methods =
	{
		&caption_text_open,
		&caption_text_geteof,
		&caption_text_seteof
	};
	
	static const node_method_set caption_text_methods =
	{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		&caption_text_data_methods
	};
	
	static FSTreePtr caption_text_factory( const FSTree*        parent,
	                                       const plus::string&  name,
	                                       const void*          args )
	{
		return new FSTree( parent, name, S_IFREG | 0600, &caption_text_methods );
	}
	
	
	namespace
	{
		
		bool& Wrapped( const FSTree* view )
		{
			return gCaptionParametersMap[ view ].itIsWrapped;
		}
		
		bool& Disabling( const FSTree* view )
		{
			return gCaptionParametersMap[ view ].disabling;
		}
		
	}
	
	typedef View_Property< plus::serialize_bool, Wrapped   > Wrapped_Property;
	typedef View_Property< plus::serialize_bool, Disabling > Disabling_Property;
	
	#define PROPERTY( prop )  &new_property, &property_params_factory< prop >::value
	
	static const vfs::fixed_mapping local_mappings[] =
	{
		{ "text", &caption_text_factory },
		
		{ "wrapped",   PROPERTY( Wrapped_Property   ) },
		{ "disabling", PROPERTY( Disabling_Property ) },
		
		{ NULL, NULL }
	};
	
	FSTreePtr New_FSTree_new_caption( const FSTree*        parent,
	                                  const plus::string&  name,
	                                  const void*          args )
	{
		return New_new_view( parent,
		                     name,
		                     &CreateView,
		                     local_mappings,
		                     &DestroyDelegate );
	}
	
}


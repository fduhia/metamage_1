/*	==========
 *	Scribes.hh
 *	==========
 */

#ifndef GENIE_FILESYSTEM_SCRIBES_HH
#define GENIE_FILESYSTEM_SCRIBES_HH

// Standard C++
#include <algorithm>

// Mac OS
#ifndef __QUICKDRAW__
#include <Quickdraw.h>
#endif

// Iota
#include "iota/decimal.hh"

// plus
#include "plus/string.hh"


namespace Genie
{
	
	class WriteLengthIsWrongSizeForObject {};
	
	class Invalid_Decode_Input {};
	
	
	template < class Scribe >
	plus::string Freeze( const typename Scribe::Value& value, bool binary )
	{
		return binary ? plus::string( (const char*) &value, sizeof value )
		              : Scribe::Encode( value );
	}
	
	template < class Scribe >
	typename Scribe::Value Vivify( const char* begin, const char* end, bool binary )
	{
		typedef typename Scribe::Value POD;
		
		POD result;
		
		if ( binary )
		{
			if ( end - begin != sizeof result )
			{
				throw WriteLengthIsWrongSizeForObject();
			}
			
			std::copy( begin, end, (char*) &result );
		}
		else
		{
			result = Scribe::Decode( begin, end );
		}
		
		return result;
	}
	
	
	struct Boolean_Scribe
	{
		typedef bool Value;
		
		static plus::string Encode( bool value )
		{
			return value ? "1" : "0";
		}
		
		static bool Decode( const char* begin, const char* end )
		{
			return begin[ 0 ] != '0';
		}
	};
	
	struct Int_Scribe
	{
		typedef int Value;
		
		static plus::string Encode( int value )
		{
			return iota::inscribe_decimal( value );
		}
		
		static int Decode( const char* begin, const char* end )
		{
			// *end == '\n'
			
			return iota::parse_decimal( begin );
		}
	};
	
	template < class Int >
	struct Integer_Scribe
	{
		typedef Int Value;
		
		static plus::string Encode( Int value )
		{
			return Int_Scribe::Encode( value );
		}
		
		static Int Decode( const char* begin, const char* end )
		{
			return Int( Int_Scribe::Decode( begin, end ) );
		}
	};
	
	
	Point ReadPoint( const char* begin, const char* end );
	
	plus::string WritePoint( Point point, char separator );
	
	template < char separator = ',' >
	struct Point_Scribe
	{
		typedef Point Value;
		
		static plus::string Encode( Point value )
		{
			return WritePoint( value, separator );
		}
		
		static Point Decode( const char* begin, const char* end )
		{
			return ReadPoint( begin, end );
		}
	};
	
	struct Rect_Scribe
	{
		typedef Rect Value;
		
		static plus::string Encode( const Rect& value );
		
		static Rect Decode( const char* begin, const char* end );
	};
	
}

#endif


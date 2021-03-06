/*	==============
 *	name_filter.cc
 *	==============
 */

#include "recall/name_filter.hh"

// Standard C++
#include <algorithm>

// plus
#include "plus/var_string.hh"


#define STR_LEN( string )  "" string, (sizeof string - 1)

#define ARRAY_LEN( array )  ( sizeof array / sizeof array[0] )


namespace recall
{
	
	struct replacement
	{
		const char*  pattern;
		std::size_t  pattern_length;
		const char*  new_text;
		std::size_t  new_text_length;
	};
	
	#define REPLACE( newtext, pattern )  { STR_LEN( pattern ), STR_LEN( newtext ) }
	
	const replacement global_replacements[] =
	{
		REPLACE( "std::string",  "std::basic_string< char, std::char_traits< char >, std::allocator< char > >" ),
		
		REPLACE( "std::vector< std::string >",  "std::vector< std::string, std::allocator< std::string > >" )
	};
	
	void filter_symbol( plus::var_string& name )
	{
		const replacement* end = global_replacements + ARRAY_LEN( global_replacements );
		
		for ( const replacement* it = global_replacements;  it < end;  ++it )
		{
			const char* pattern = it->pattern;
			const char* pattern_end = pattern + it->pattern_length;
			
			while ( true )
			{
				plus::var_string::iterator found = std::search( name.begin(),
				                                                name.end(),
				                                                pattern,
				                                                pattern_end );
				
				if ( found == name.end() )
				{
					break;
				}
				
				name.replace( found, found + it->pattern_length, it->new_text, it->new_text_length );
			}
		}
	}
	
}


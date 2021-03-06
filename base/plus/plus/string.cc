/*
	string.cc
	---------
	
	Some of the find-related member functions were written using 
	boost/shmem/containers/string.hpp as a guide.
*/

#include "plus/string.hh"

// Standard C++
#include <algorithm>
#include <stdexcept>

// Standard C
#include <stdlib.h>
#include <string.h>

// debug
#include "debug/assert.hh"

// plus
#include "plus/string_details.hh"


namespace plus
{
	
	const string string::null;
	
	
	static int lexicographical_compare_3way( const char* a, const char* a_end,
	                                         const char* b, const char* b_end )
	{
		while ( a < a_end  &&  b < b_end )
		{
			if ( const int diff = *a++ - *b++ )
			{
				return diff;
			}
		}
		
		return (a != a_end) - (b != b_end);
	}
	
	static int string_compare( const char* a, size_t a_size,
	                           const char* b, size_t b_size )
	{
		return lexicographical_compare_3way( a, a + a_size,
		                                     b, b + b_size );
	}
	
	static int string_equal( const char* a, size_t a_size,
	                         const char* b, size_t b_size )
	{
		return a_size == b_size  &&  string_compare( a, a_size,
		                                             b, b_size ) == 0;
	}
	
	
	class matches_any
	{
		private:
			const char*  begin;
			const char*  end;
		
		public:
			matches_any( const char* a, const char* z ) : begin( a ), end( z )
			{
			}
			
			bool operator()( char c ) const
			{
				return std::find( begin, end, c ) != end;
			}
	};
	
	class matches_none
	{
		private:
			const char*  begin;
			const char*  end;
		
		public:
			matches_none( const char* a, const char* z ) : begin( a ), end( z )
			{
			}
			
			bool operator()( char c ) const
			{
				return std::find( begin, end, c ) == end;
			}
	};
	
	
	template < class Iter, class F >
	static Iter find_last_if( Iter begin, Iter end, F f )
	{
		ASSERT( begin <= end );
		
		Iter it = end;
		
		while ( --it >= begin )
		{
			if ( f( *it ) )
			{
				return it;
			}
		}
		
		return end;
	}
	
	
	string::string( const char*    p,
	                size_type      length,
	                delete_policy  policy,
	                size_type      capacity )
	{
		check_size( length   );
		check_size( capacity );
		
		store.alloc.pointer  = p;
		store.alloc.length   = length;
		store.alloc.capacity = capacity;  // may be zero
		
		_policy( ~policy );
	}
	
	string::string( const char* p, size_type length )
	{
		ASSERT( p != NULL );
		
		ASSERT( p + length >= p );
		
		check_size( length );
		
		allocate_data( store, p, length );
	}
	
	string::string( const char* p, const char* q )
	{
		store.small[ 0          ] = '\0';
		store.small[ max_offset ] = max_offset;
		
		assign( p, q );
	}
	
	string::string( const char* s )
	{
		store.small[ 0          ] = '\0';
		store.small[ max_offset ] = max_offset;
		
		assign( s );
	}
	
	string::string( size_type n, char c )
	{
		// An uninitialized 15-char string is acceptable because we're just
		// going to assign over it anyway, and empties aren't faster here.
		store.small[ max_offset ] = 0;
		
		assign( n, c );
	}
	
	string::string( const string& other, size_type pos, size_type n )
	{
		construct_from_move( store, other.substr( pos, n ).move() );
	}
	
	const char* string::c_str() const
	{
		if ( is_small() )
		{
			return store.small;  // always terminated
		}
		
		const char* begin = store.alloc.pointer + alloc_substr_offset( store );
		
		if ( begin[ store.alloc.length ] == '\0' )
		{
			return begin;
		}
		
		datum_storage temp;
		
		char* p = allocate_data( temp, begin, store.alloc.length );
		
		datum_storage& u = const_cast< datum_storage& >( store );
		
		destroy( u );
		
		u = temp;
		
		ASSERT( !is_small() );
		
		return p;
	}
	
	const char* string::end() const
	{
		return begin() + size();
	}
	
	string& string::assign( const char* p, size_type length, delete_policy policy, size_type capacity )
	{
		check_size( length   );
		check_size( capacity );
		
		if ( length )
		{
			ASSERT( p != NULL );
			
			ASSERT( p + length >= p );
		}
		
		destroy( store );
		
		store.alloc.pointer  = p;
		store.alloc.length   = length;
		store.alloc.capacity = capacity;  // may be zero
		
		_policy( ~policy );
		
		return *this;
	}
	
	char* string::reset( size_type length )
	{
		check_size( length );
		
		return reallocate( store, length );
	}
	
	string& string::assign( const char* p, size_type length )
	{
		return assign( string( p, length ).move() );
	}
	
	string& string::assign( const char* s )
	{
		ASSERT( s != NULL );
		
		const size_type length = strlen( s );
		
		return assign( s, length );
	}
	
	string& string::assign( size_type n, char c )
	{
		// reset() will throw if n exceeds max_size()
		
		char* pointer = reset( n );
		
		memset( pointer, c, n );
		
		return *this;
	}
	
	string& string::assign( datum_movable& m )
	{
		if ( &m != &store )
		{
			destroy( store );
			
			construct_from_move_untaint( store, m );
		}
		
		return *this;
	}
	
	string& string::assign( const string& other, size_type pos, size_type n )
	{
		return assign( other.substr( pos, n ).move() );
	}
	
	
	string::size_type string::copy( char* buffer, size_type n, size_type pos ) const
	{
		const size_t size = length();
		
		if ( pos > size )
		{
			throw std::out_of_range( "plus::string" );
		}
		
		n = std::min( n, size - pos );
		
		memcpy( buffer, data() + pos, n );
		
		return n;
	}
	
	
	string::size_type string::find( const string& s, size_type pos ) const
	{
		return find( s.data(), pos, s.size() );
	}
	
	string::size_type string::find( const char* s, size_type pos ) const
	{
		return find( s, pos, strlen( s ) );
	}
	
	string::size_type string::find( const char* s, size_type pos, size_type n ) const
	{
		const size_t size = length();
		
		if ( pos > size  ||  pos + n > size )
		{
			return npos;
		}
		
		if ( n == 0 )
		{
			return pos;
		}
		
		const char* begin = data();
		const char* end   = begin + size;
		
		const char* it = std::search( begin + pos, end, s, s + n );
		
		return it != end ? it - begin : npos;
	}
	
	string::size_type string::find( char c, size_type pos ) const
	{
		return find( &c, pos, 1 );
	}
	
	
	string::size_type string::rfind( const string& s, size_type pos ) const
	{
		return rfind( s.data(), pos, s.size() );
	}
	
	string::size_type string::rfind( const char* s, size_type pos ) const
	{
		return rfind( s, pos, strlen( s ) );
	}
	
	string::size_type string::rfind( const char* s, size_type pos, size_type n ) const
	{
		const size_type size = length();
		
		if ( n > size )
		{
			return npos;
		}
		
		pos = std::min( pos, size - n );
		
		if ( n == 0 )
		{
			return pos;
		}
		
		const char* begin = data();
		const char* end   = begin + pos + n;
		
		const char* it = std::find_end( begin, end, s, s + n );
		
		return it != end ? it - begin : npos;
	}
	
	string::size_type string::rfind( char c, size_type pos ) const
	{
		return rfind( &c, pos, 1 );
	}
	
	
	string::size_type string::find_first_of( const string& s, size_type pos ) const
	{
		return find_first_of( s.data(), pos, s.size() );
	}
	
	string::size_type string::find_first_of( const char* s, size_type pos ) const
	{
		return find_first_of( s, pos, strlen( s ) );
	}
	
	string::size_type string::find_first_of( const char* s, size_type pos, size_type n ) const
	{
		const size_t size = length();
		
		if ( pos >= size )
		{
			return npos;
		}
		
		const char* begin = data();
		const char* end   = begin + size;
		
		const char* it = std::find_first_of( begin + pos, end, s, s + n );
		
		return it != end ? it - begin : npos;
	}
	
	string::size_type string::find_first_of( char c, size_type pos ) const
	{
		return find( c, pos );
	}
	
	
	string::size_type string::find_last_of( const string& s, size_type pos ) const
	{
		return find_last_of( s.data(), pos, s.size() );
	}
	
	string::size_type string::find_last_of( const char* s, size_type pos ) const
	{
		return find_last_of( s, pos, strlen( s ) );
	}
	
	string::size_type string::find_last_of( const char* s, size_type pos, size_type n ) const
	{
		const size_type size = length();
		
		if ( 1 > size )
		{
			return npos;
		}
		
		pos = std::min( pos, size - 1 );
		
		const char* begin = data();
		const char* end   = begin + pos + 1;
		
		const char* it = find_last_if( begin, end, matches_any( s, s + n ) );
		
		return it != end ? it - begin : npos;
	}
	
	string::size_type string::find_last_of( char c, size_type pos ) const
	{
		return find_last_of( &c, pos, 1 );
	}
	
	
	string::size_type string::find_first_not_of( const string& s, size_type pos ) const
	{
		return find_first_not_of( s.data(), pos, s.size() );
	}
	
	string::size_type string::find_first_not_of( const char* s, size_type pos ) const
	{
		return find_first_not_of( s, pos, strlen( s ) );
	}
	
	string::size_type string::find_first_not_of( const char* s, size_type pos, size_type n ) const
	{
		const size_t size = length();
		
		if ( pos >= size )
		{
			return npos;
		}
		
		const char* begin = data();
		const char* end   = begin + size;
		
		const char* it = std::find_if( begin + pos, end, matches_none( s, s + n ) );
		
		return it != end ? it - begin : npos;
	}
	
	string::size_type string::find_first_not_of( char c, size_type pos ) const
	{
		return find_first_not_of( &c, pos, 1 );
	}
	
	
	string::size_type string::find_last_not_of( const string& s, size_type pos ) const
	{
		return find_last_not_of( s.data(), pos, s.size() );
	}
	
	string::size_type string::find_last_not_of( const char* s, size_type pos ) const
	{
		return find_last_not_of( s, pos, strlen( s ) );
	}
	
	string::size_type string::find_last_not_of( const char* s, size_type pos, size_type n ) const
	{
		const size_type size = length();
		
		if ( 1 > size )
		{
			return npos;
		}
		
		pos = std::min( pos, size - 1 );
		
		const char* begin = data();
		const char* end   = begin + pos + 1;
		
		const char* it = find_last_if( begin, end, matches_none( s, s + n ) );
		
		return it != end ? it - begin : npos;
	}
	
	string::size_type string::find_last_not_of( char c, size_type pos ) const
	{
		return find_last_not_of( &c, pos, 1 );
	}
	
	
	string string::substr( size_type pos, size_type n ) const
	{
		const size_type len = size();
		
		if ( pos > len )
		{
			throw std::out_of_range( "plus::string" );
		}
		
		if ( len - pos < n )
		{
			n = len - pos;
		}
		
		if ( n > datum_max_offset  &&  _policy() >= ~delete_shared )
		{
			plus::string temp = *this;
			
			const long offset = alloc_substr_offset( store );
			
			temp.store.alloc.length   = n;
			temp.store.alloc.capacity = -(offset + pos);
			
			return temp.move();
		}
		
		return string( data() + pos, n );
	}
	
	
	int string::compare( const string& b ) const
	{
		return string_compare( data(), size(), b.data(), b.size() );
	}
	
	int string::compare( size_type a_pos, size_type a_n, const string& b ) const
	{
		const size_type a_size = size();
		
		if ( a_pos > a_size )
		{
			throw std::out_of_range( "plus::string" );
		}
		
		a_n = std::min( a_n, a_size - a_pos );
		
		return string_compare( data() + a_pos, a_n, b.data(), b.size() );
	}
	
	int string::compare( size_type a_pos, size_type a_n, const string& b, size_type b_pos, size_type b_n ) const
	{
		const size_type a_size = size();
		
		if ( a_pos > a_size )
		{
			throw std::out_of_range( "plus::string" );
		}
		
		a_n = std::min( a_n, a_size - a_pos );
		
		const size_type b_size = b.size();
		
		if ( b_pos > b_size )
		{
			throw std::out_of_range( "plus::string" );
		}
		
		b_n = std::min( b_n, b_size - b_pos );
		
		return string_compare( data() + a_pos, a_n, b.data() + b_pos, b_n );
	}
	
	int string::compare( const char* b ) const
	{
		return string_compare( data(), size(), b, strlen( b ) );
	}
	
	int string::compare( size_type a_pos, size_type a_n, const char* b, size_type b_n ) const
	{
		const size_type a_size = size();
		
		if ( a_pos > a_size )
		{
			throw std::out_of_range( "plus::string" );
		}
		
		a_n = std::min( a_n, a_size - a_pos );
		
		b_n = std::min< size_type >( b_n, strlen( b ) );
		
		return string_compare( data(), size(), b, b_n );
	}
	
	
	bool operator==( const string& a, const string& b )
	{
		return string_equal( a.data(), a.size(), b.data(), b.size() );
	}
	
	bool operator==( const string& a, const char* b )
	{
		return string_equal( a.data(), a.size(), b, strlen( b ) );
	}
	
	
	bool operator<( const string& a, const string& b )
	{
		return string_compare( a.data(), a.size(), b.data(), b.size() ) < 0;
	}
	
	bool operator<( const string& a, const char* b )
	{
		return string_compare( a.data(), a.size(), b, strlen( b ) ) < 0;
	}
	
	bool operator<( const char* a, const string& b )
	{
		return string_compare( a, strlen( a ), b.data(), b.size() ) < 0;
	}
	
}


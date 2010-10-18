// ===========
// Builtins.cc
// ===========

#include "Builtins.hh"

// Standard C++
#include <algorithm>
#include <functional>
#include <map>
#include <set>

// Standard C/C++
#include <cstring>

// Standard C
#include <stdlib.h>

// POSIX
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

// Iota
#include "iota/decimal.hh"
#include "iota/strings.hh"

// more-posix
#include "more/perror.hh"

// plus
#include "plus/var_string.hh"

// poseven
#include "poseven/functions/fcntl.hh"
#include "poseven/functions/open.hh"
#include "poseven/functions/write.hh"

// sh
#include "Options.hh"
#include "PositionalParameters.hh"
#include "Execution.hh"
#include "ReadExecuteLoop.hh"


extern "C" char** environ;


namespace tool
{
	
	namespace n = nucleus;
	namespace p7 = poseven;
	
	
	typedef std::map< plus::string, plus::string > StringMap;
	
	static StringMap gLocalVariables;
	static StringMap gAliases;
	
	static std::set< plus::string > gVariablesToExport;
	
	static void PrintVariable( const StringMap::value_type& var )
	{
		const plus::string& name  = var.first;
		const plus::string& value = var.second;
		
		plus::var_string variable = name;
		
		variable += "='";
		variable +=    value;
		variable +=        "'\n";
		
		p7::write( p7::stdout_fileno, variable );
	}
	
	static void PrintAlias( const StringMap::value_type& var )
	{
		const plus::string& name  = var.first;
		const plus::string& value = var.second;
		
		plus::var_string alias = "alias ";
		
		alias += name;
		alias +=    "='";
		alias +=       value;
		alias +=           "'\n";
		
		p7::write( p7::stdout_fileno, alias );
	}
	
	
	static bool UnmarkVariableForExport( const plus::string& name )
	{
		std::set< plus::string >::iterator found = gVariablesToExport.find( name );
		
		bool wasMarked = found != gVariablesToExport.end();
		
		if ( wasMarked )
		{
			gVariablesToExport.erase( found );
		}
		
		return wasMarked;
	}
	
	void AssignShellVariable( const char* name, const char* value )
	{
		if ( getenv( name ) || UnmarkVariableForExport( name ) )
		{
			// Variable already exists in environment, or was marked for export
			setenv( name, value, 1 );
		}
		else
		{
			// Not set, or set locally
			gLocalVariables[ name ] = value;
		}
	}
	
	const char* QueryShellVariable( const plus::string& name )
	{
		StringMap::const_iterator found = gLocalVariables.find( name );
		
		if ( found != gLocalVariables.end() )
		{
			return found->second.c_str();
		}
		
		return NULL;
	}
	
	// Builtins.  argc is guaranteed to be positive.
	
	static p7::exit_t Builtin_CD( int argc, char** argv )
	{
		const char* dir = argv[1];
		
		if ( argc == 1 )
		{
			if ( const char* home = getenv( "HOME" ) )
			{
				dir = home;
			}
			else
			{
				dir = "/";
			}
		}
		
		int changed = chdir( dir );
		
		if ( changed < 0 )
		{
			more::perror( "cd", dir );
			
			return p7::exit_failure;
		}
		
		// Apparently setenv() breaks something.
		//setenv( "OLDPWD", getenv( "PWD" ), 1 );
		
		// FIXME:  This should be full pathname.
		//setenv( "PWD", argv[ 1 ], 1 );
		
		return p7::exit_success;
	}
	
	static p7::exit_t Builtin_Alias( int argc, char** argv )
	{
		if ( argc == 1 )
		{
			// $ alias
			std::for_each( gAliases.begin(),
			               gAliases.end(),
			               std::ptr_fun( PrintAlias ) );
		}
		else if ( argc == 2 )
		{
			const char* name = argv[ 1 ];
			
			if ( char* eq = std::strchr( argv[ 1 ], '=' ) )
			{
				// $ alias foo=bar
				
				// FIXME:  This is const.
				*eq = '\0';
				
				gAliases[ name ] = eq + 1;
			}
			else
			{
				// $ alias foo
				StringMap::const_iterator found = gAliases.find( name );
				
				if ( found != gAliases.end() )
				{
					PrintAlias( *found );
				}
			}
		}
		
		return p7::exit_success;
	}
	
	static p7::exit_t Builtin_Echo( int argc, char** argv )
	{
		plus::var_string line;
		
		if ( argc > 1 )
		{
			line = argv[1];
			
			for ( short i = 2; i < argc; i++ )
			{
				line += " ";
				line += argv[i];
			}
		}
		
		line += '\n';
		
		p7::write( p7::stdout_fileno, line );
		
		return p7::exit_success;
	}
	
	static p7::exit_t Builtin_Exit( int argc, char** argv )
	{
		int exitStatus = 0;
		
		if ( argc > 1 )
		{
			exitStatus = iota::parse_unsigned_decimal( argv[ 1 ] );
		}
		
		throw p7::exit_t( exitStatus );
		
		// Not reached
		return p7::exit_success;
	}
	
	static p7::exit_t Builtin_Export( int argc, char** argv )
	{
		if ( argc == 1 )
		{
			// $ export
			char** envp = environ;
			
			while ( *envp != NULL )
			{
				// Can't use the name 'export' in Metrowerks C++
				plus::var_string export_ = "export ";
				
				export_ += *envp;
				export_ += '\n';
				
				p7::write( p7::stdout_fileno, export_ );
				
				++envp;
			}
		}
		else if ( argc == 2 )
		{
			const char* const arg1 = argv[ 1 ];
			
			if ( const char* eq = std::strchr( arg1, '=' ) )
			{
				// $ export foo=bar
				plus::string name( arg1, eq - arg1 );
				
				setenv( name.c_str(), eq + 1, true );
				
				gLocalVariables.erase( name );
			}
			else
			{
				// $ export foo
				const char* var = arg1;
				
				if ( getenv( var ) == NULL )
				{
					// Environment variable unset
					StringMap::const_iterator found = gLocalVariables.find( var );
					
					if ( found != gLocalVariables.end() )
					{
						// Shell variable is set, export it
						setenv( var, found->second.c_str(), 1 );
						gLocalVariables.erase( var );
					}
					else
					{
						// Shell variable unset, mark exported
						gVariablesToExport.insert( var );
					}
				}
			}
		}
		
		return p7::exit_success;
	}
	
	static p7::exit_t Builtin_PWD( int argc, char** argv )
	{
		plus::var_string cwd;
		
		char* p = cwd.reset( 256 );
		
		while ( getcwd( p, cwd.size() ) == NULL )
		{
			p = cwd.reset( cwd.size() * 2 );
		}
		
		cwd.resize( strlen( cwd.c_str() ) );
		
		cwd += '\n';
		
		p7::write( p7::stdout_fileno, cwd );
		
		return p7::exit_success;
	}
	
	static p7::exit_t Builtin_Set( int argc, char** argv )
	{
		if ( argc == 1 )
		{
			// $ set
			std::for_each( gLocalVariables.begin(),
			               gLocalVariables.end(),
			               std::ptr_fun( PrintVariable ) );
		}
		else
		{
			const char* first = argv[1];
			
			if ( first[0] == '-'  ||  first[0] == '+' )
			{
				const bool value = first[0] == '-';
				
				if ( argc == 2  &&  first[1] == 'e' )
				{
					SetOption( kOptionExitOnError, value );
				}
				else if ( argc == 3  &&  first[1] == 'o' )
				{
					SetOptionByName( argv[2], value );
				}
			}
		}
		
		return p7::exit_success;
	}
	
	static p7::exit_t Builtin_Unalias( int argc, char** argv )
	{
		while ( --argc )
		{
			gAliases.erase( argv[ argc ] );
		}
		
		return p7::exit_success;
	}
	
	static p7::exit_t Builtin_Unset( int argc, char** argv )
	{
		while ( --argc )
		{
			gLocalVariables.erase( argv[ argc ] );
			unsetenv( argv[ argc ] );
		}
		
		return p7::exit_success;
	}
	
	class ReplacedParametersScope
	{
		private:
			std::size_t         itsSavedParameterCount;
			char const* const*  itsSavedParameters;
		
		public:
			ReplacedParametersScope( std::size_t count, char const* const* params )
			:
				itsSavedParameterCount( gParameterCount ),
				itsSavedParameters    ( gParameters     )
			{
				gParameterCount = count;
				gParameters     = params;
			}
			
			~ReplacedParametersScope()
			{
				gParameterCount = itsSavedParameterCount;
				gParameters     = itsSavedParameters;
			}
	};
	
	static p7::exit_t BuiltinDot( int argc, char** argv )
	{
		if ( argc < 2 )
		{
			p7::write( p7::stderr_fileno, STR_LEN( "sh: .: filename argument required\n" ) );
			
			return p7::exit_t( 2 );
		}
		
	#ifdef O_CLOEXEC
		
		const p7::open_flags_t flags = p7::o_rdonly | p7::o_cloexec;
		
	#else
		
		const p7::open_flags_t flags = p7::o_rdonly;
		
	#endif
		
		n::owned< p7::fd_t > fd = p7::open( argv[ 1 ], flags );
		
	#ifndef O_CLOEXEC
		
		p7::fcntl< p7::f_setfd >( fd, p7::fd_cloexec );
		
	#endif
		
		ReplacedParametersScope dotParams( argc - 2, argv + 2 );
		
		return n::convert< p7::exit_t >( ReadExecuteLoop( fd, false ) );
	}
	
	struct builtin_t
	{
		const char*    name;
		const Builtin  code;
	};
	
	static inline bool operator==( const builtin_t& b, const plus::string& s )
	{
		return b.name == s;
	}
	
	static const builtin_t builtins[] =
	{
		{ "alias",   Builtin_Alias   },
		{ "cd",      Builtin_CD      },
		{ "echo",    Builtin_Echo    },
		{ "exit",    Builtin_Exit    },
		{ "export",  Builtin_Export  },
		{ "pwd",     Builtin_PWD     },
		{ "set",     Builtin_Set     },
		{ "unalias", Builtin_Unalias },
		{ "unset",   Builtin_Unset   },
		{ ".",       BuiltinDot      }
	};
	
	Builtin FindBuiltin( const plus::string& name )
	{
		const builtin_t* begin = builtins;
		const builtin_t* end   = begin + sizeof builtins / sizeof builtins[0];
		
		const builtin_t* it = std::find( begin, end, name );
		
		return it != end ? it->code : NULL;
	}
	
}


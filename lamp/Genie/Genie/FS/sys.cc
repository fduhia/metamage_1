/*
	Genie/FS/sys.cc
	---------------
*/

#include "Genie/FS/sys.hh"

// POSIX
#include <sys/stat.h>

// Standard C++
#include <algorithm>

// gear
#include "gear/hexidecimal.hh"

// Nitrogen
#include "Mac/Sound/Functions/SysBeep.hh"

// poseven
#include "poseven/types/errno_t.hh"

// Genie
#include "Genie/code/fixed_address.hh"
#include "Genie/FS/basic_directory.hh"
#include "Genie/FS/FSTree.hh"
#include "Genie/FS/FSTreeCache.hh"
#include "Genie/FS/file_method_set.hh"
#include "Genie/FS/node_method_set.hh"
#include "Genie/FS/premapped.hh"
#include "Genie/FS/sys/app.hh"
#include "Genie/FS/sys/cpu.hh"
#include "Genie/FS/sys/mac.hh"
#include "Genie/FS/sys/type.hh"
#include "Genie/SystemCallRegistry.hh"


namespace Genie
{
	
	namespace p7 = poseven;
	
	
	typedef const SystemCall* SystemCallPtr;
	
	
	static plus::string name_of_syscall( SystemCallPtr key )
	{
		if ( key->name == NULL )
		{
			plus::string name;
			
			char* p = name.reset( sizeof '.' + sizeof (unsigned) * 2 );  // 9
			
			p[0] = '.';
			
			gear::encode_32_bit_hex( (unsigned) key, &p[1] );
			
			return name;
		}
		
		return key->name;
	}
	
	
	static shared_exec_handle builtin_loadexec( const FSTree* node );
	
	static const file_method_set builtin_file_methods =
	{
		NULL,
		NULL,
		NULL,
		&builtin_loadexec
	};
	
	static const node_method_set builtin_methods =
	{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		&builtin_file_methods
	};
	
	static shared_exec_handle builtin_loadexec( const FSTree* node )
	{
		relix_entry& extra = *(relix_entry*) node->extra();
		
		return new fixed_address( extra );
	}
	
	
	static FSTreePtr syscall_lookup( const FSTreePtr& parent, const plus::string& name )
	{
		if ( LookUpSystemCallByName( name.c_str() ) == NULL )
		{
			poseven::throw_errno( ENOENT );
		}
		
		return Premapped_Factory( parent, name );
	}
	
	class syscall_IteratorConverter
	{
		public:
			vfs::dir_entry operator()( const SystemCall& value ) const
			{
				const ino_t inode = &value - GetSystemCall( 0 );
				
				plus::string name = name_of_syscall( &value );
				
				return vfs::dir_entry( inode, name );
			}
	};
	
	static void syscall_iterate( const FSTreePtr& parent, FSTreeCache& cache )
	{
		syscall_IteratorConverter converter;
		
		std::transform( gSystemCallArray,
		                gSystemCallArray + gLastSystemCall,
		                std::back_inserter( cache ),
		                converter );
	}
	
	static FSTreePtr New_sys_kernel_syscall( const FSTreePtr&     parent,
	                                         const plus::string&  name,
	                                         const void*          args )
	{
		return new_basic_directory( parent, name, syscall_lookup, syscall_iterate );
	}
	
	
	namespace
	{
		
		int main_true()
		{
			return 0;
		}
		
		int main_false()
		{
			return 1;
		}
		
		int main_beep()
		{
			Mac::SysBeep();
			
			return 0;
		}
		
	}
	
	static FSTreePtr Executable_Factory( const FSTreePtr&     parent,
	                                     const plus::string&  name,
	                                     const void*          args )
	{
		FSTree* result = new FSTree( parent,
		                             name,
		                             S_IFREG | 0500,
		                             &builtin_methods,
		                             sizeof (relix_entry) );
		
		relix_entry& extra = *(relix_entry*) result->extra();
		
		extra = (relix_entry) args;
		
		return result;
	}
	
	#define EXEC( main )  &Executable_Factory, (const void*) &main
	
	extern const premapped::mapping sys_kernel_bin_Mappings[];
	
	const premapped::mapping sys_kernel_bin_Mappings[] =
	{
		{ "true",  EXEC( main_true  ) },
		{ "false", EXEC( main_false ) },
		
		{ "beep",  EXEC( main_beep  ) },
		
		{ NULL, NULL }
	};
	
	
	#define PREMAPPED( map )  &premapped_factory, (const void*) map
	
	extern const premapped::mapping sys_kernel_Mappings[];
	
	const premapped::mapping sys_kernel_Mappings[] =
	{
		{ "bin",     PREMAPPED( sys_kernel_bin_Mappings ) },
		
		{ "syscall", &New_sys_kernel_syscall },
		
		{ NULL, NULL }
	};
	
	const premapped::mapping sys_Mappings[] =
	{
		{ "app",    PREMAPPED( sys_app_Mappings    ) },
		{ "cpu",    PREMAPPED( sys_cpu_Mappings    ) },
		{ "kernel", PREMAPPED( sys_kernel_Mappings ) },
		{ "mac",    PREMAPPED( sys_mac_Mappings    ) },
		{ "type",   PREMAPPED( sys_type_Mappings   ) },
		
		{ NULL, NULL }
	};
	
}


/*
	xv68k.cc
	--------
*/

// Standard C
#include <signal.h>
#include <stdlib.h>
#include <string.h>

// POSIX
#include <fcntl.h>
#include <unistd.h>

// v68k
#include "v68k/endian.hh"

// v68k-mac
#include "v68k-mac/trap_dispatcher.hh"

// v68k-user
#include "v68k-user/load.hh"

// v68k-callbacks
#include "callback/bridge.hh"

// v68k-syscalls
#include "syscall/bridge.hh"
#include "syscall/handler.hh"

// xv68k
#include "memory.hh"


#pragma exceptions off


using v68k::big_longword;


/*
	Memory map
	----------
	
	0K	+-----------------------+
		| System vectors        |
	1K	+-----------------------+
		| OS trap table         |
	2K	+-----------------------+
		| OS / supervisor stack |
	3K	+-----------------------+
		|                       |
	4K	|                       |
		|                       |
		| Toolbox trap table    |
		|                       |
		|                       |
		|                       |
	7K	+-----------------------+
		| boot code             |
	8K	+-----------------------+
		|                       |
		|                       |
		|                       |
		| argc/argv/envp params |
		|                       |
		|                       |
		|                       |
	12K	+-----------------------+
		|                       |
		|                       |
		|                       |
		| user stack            |
		|                       |
		|                       |
		|                       |
	16K	+-----------------------+
		|                       |
		|                       |
		|                       |
		|                       |
		|                       |
		|                       |
		|                       |
		= user code             =  x6
		|                       |
		|                       |
		|                       |
		|                       |
		|                       |
		|                       |
		|                       |
	64K	+-----------------------+
	
*/

const uint32_t params_max_size = 4096;
const uint32_t code_max_size   = 48 * 1024;

const uint32_t os_address   = 2048;
const uint32_t boot_address = 7168;
const uint32_t initial_SSP  = 3072;
const uint32_t initial_USP  = 16384;
const uint32_t code_address = 16384;

const uint32_t os_trap_table_address = 1024;
const uint32_t tb_trap_table_address = 3072;

const uint32_t os_trap_count = 1 <<  8;  //  256, 1K
const uint32_t tb_trap_count = 1 << 10;  // 1024, 4K

const uint32_t mem_size = code_address + code_max_size;

const uint32_t params_addr = 8192;

const uint32_t user_pb_addr   = params_addr +  0;  // 20 bytes
const uint32_t system_pb_addr = params_addr + 20;  // 20 bytes

const uint32_t argc_addr = params_addr + 40;  // 4 bytes
const uint32_t argv_addr = params_addr + 44;  // 4 bytes
const uint32_t args_addr = params_addr + 48;


static void init_trap_table( uint32_t* table, uint32_t* end, uint32_t address )
{
	const uint32_t unimplemented = v68k::big_longword( address );
	
	for ( uint32_t* p = table;  p < end;  ++p )
	{
		*p = unimplemented;
	}
}


static const uint16_t loader_code[] =
{
	0x027C,  // ANDI #DFFF,SR  ; clear S
	0xDFFF,
	
	0x4FF8,  // LEA  (3072).W,A7
	initial_USP,
	
	0x42B8,  // CLR.L  user_pb_addr + 2 * sizeof (uint32_t)  ; user_pb->errno_var
	user_pb_addr + 2 * sizeof (uint32_t),
	
	0x21FC,  // MOVE.L  #user_pb_addr,(system_pb_addr).W  ; pb->current_user
	0x0000,
	user_pb_addr,
	system_pb_addr,
	
	0x4878,  // PEA  (system_pb_addr).W  ; pb
	system_pb_addr,
	
	0x4878,  // PEA  (0).W  ; envp
	0x0000,
	
	0x2F38,  // MOVE.L  (argv_addr).W,-(A7)
	argv_addr,
	
	0x2F38,  // MOVE.L  (argc_addr).W,-(A7)
	argc_addr,
	
	0x4EB8,  // JSR  code_address
	code_address,
	
	0x2F00,  // MOVE.L  D0,-(A7)
	0x7001,  // MOVEQ  #1,D0
	0x484A,  // BKPT   #2
	
	0x4E75   // RTS
};

#define HANDLER( handler )  handler, sizeof handler

static void load_vectors( v68k::user::os_load_spec& os )
{
	uint32_t* vectors = (uint32_t*) os.mem_base;
	
	memset( vectors, 0xFF, 1024 );
	
	vectors[0] = big_longword( initial_SSP );  // isp
	
	using v68k::user::install_exception_handler;
	using v68k::mac::trap_dispatcher;
	
	install_exception_handler( os, 10, HANDLER( trap_dispatcher ) );
	install_exception_handler( os, 32, HANDLER( system_call ) );
	
	os.mem_used = boot_address;
	
	install_exception_handler( os,  1, HANDLER( loader_code ) );
	
	using namespace v68k::callback;
	
	vectors[ 4] = big_longword( callback_address( illegal_instruction ) );
	vectors[ 5] = big_longword( callback_address( division_by_zero    ) );
	vectors[ 8] = big_longword( callback_address( privilege_violation ) );
	vectors[11] = big_longword( callback_address( line_F_emulator     ) );
}

static int execute_68k( int argc, char** argv )
{
	const char* path = argv[1];
	
	const char* instruction_limit_var = getenv( "XV68K_INSTRUCTION_LIMIT" );
	
	const int instruction_limit = instruction_limit_var ? atoi( instruction_limit_var ) : 0;
	
	errno_ptr_addr = params_addr + 2 * sizeof (uint32_t);
	
	uint8_t* mem = (uint8_t*) calloc( 1, mem_size );
	
	if ( mem == NULL )
	{
		abort();
	}
	
	v68k::user::os_load_spec load = { mem, mem_size, os_address };
	
	load_vectors( load );
	
	uint32_t* os_traps = (uint32_t*) &mem[ os_trap_table_address ];
	uint32_t* tb_traps = (uint32_t*) &mem[ tb_trap_table_address ];
	
	const uint32_t unimplemented = callback_address( v68k::callback::unimplemented_trap );
	
	init_trap_table( os_traps, os_traps + os_trap_count, unimplemented );
	init_trap_table( tb_traps, tb_traps + tb_trap_count, unimplemented );
	
	using namespace v68k::callback;
	
	os_traps[ 0x1E ] = big_longword( callback_address( NewPtr_trap     ) );
	os_traps[ 0x1F ] = big_longword( callback_address( DisposePtr_trap ) );
	
	const uint32_t big_no_op = big_longword( callback_address( v68k::callback::no_op ) );
	
	os_traps[ 0x46 ] = big_no_op;  // GetTrapAddress
	os_traps[ 0x47 ] = big_no_op;  // SetTrapAddress
	os_traps[ 0x55 ] = big_no_op;  // StripAddress
	os_traps[ 0x98 ] = big_no_op;  // HWPriv
	
	tb_traps[ 0x01C8 ] = big_longword( callback_address( SysBeep_trap     ) );
	tb_traps[ 0x01F4 ] = big_longword( callback_address( ExitToShell_trap ) );
	
	(uint32_t&) mem[ argc_addr ] = big_longword( argc - 1 );
	(uint32_t&) mem[ argv_addr ] = big_longword( args_addr );
	
	uint32_t* args = (uint32_t*) &mem[ args_addr ];
	
	uint8_t* args_limit = &mem[ params_addr ] + params_max_size;
	
	uint8_t* args_data = (uint8_t*) (args + argc);
	
	if ( args_data >= args_limit )
	{
		abort();
	}
	
	while ( *++argv != NULL )
	{
		*args++ = big_longword( args_data - mem );
		
		const size_t len = strlen( *argv ) + 1;
		
		if ( len > args_limit - args_data )
		{
			abort();
		}
		
		memcpy( args_data, *argv, len );
		
		args_data += len;
	}
	
	*args = 0;  // trailing NULL of argv
	
	int fd;
	
	if ( path != NULL )
	{
		fd = open( path, O_RDONLY );
		
		if ( fd < 0 )
		{
			return 1;
		}
	}
	else
	{
		fd = STDIN_FILENO;
	}
	
	int n_read = read( fd, mem + code_address, code_max_size );
	
	if ( path != NULL )
	{
		close( fd );
	}
	
	const memory_manager memory( mem, mem_size );
	
	v68k::emulator emu( v68k::mc68000, memory );
	
	emu.reset();
	
step_loop:
	
	while ( emu.step() )
	{
		if ( instruction_limit != 0  &&  emu.instruction_count() > instruction_limit )
		{
			raise( SIGXCPU );
		}
		
		continue;
	}
	
	if ( emu.condition == v68k::bkpt_2 )
	{
		if ( bridge_call( emu ) )
		{
			emu.acknowledge_breakpoint( 0x4E75 );  // RTS
		}
		
		goto step_loop;
	}
	
	if ( emu.condition == v68k::bkpt_3 )
	{
		if ( uint32_t new_opcode = v68k::callback::bridge( emu ) )
		{
			emu.acknowledge_breakpoint( new_opcode );
		}
		
		goto step_loop;
	}
	
	switch ( emu.condition )
	{
		using namespace v68k;
		
		case halted:
			raise( SIGSEGV );
			break;
		
		case bkpt_0:
		case bkpt_1:
		case bkpt_2:
		case bkpt_3:
		case bkpt_4:
		case bkpt_5:
		case bkpt_6:
		case bkpt_7:
			raise( SIGILL );
			break;
		
		default:
			break;
	}
	
	return 1;
}

int main( int argc, char** argv )
{
	return execute_68k( argc, argv );
}


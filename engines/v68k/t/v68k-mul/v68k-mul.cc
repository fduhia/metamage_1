/*
	v68k-mul.cc
	-----------
*/

// Standard C
#include <string.h>

// v68k
#include "v68k/emulator.hh"
#include "v68k/endian.hh"

// tap-out
#include "tap/test.hh"


#pragma exceptions off


static const unsigned n_tests = 6 + 6;


using v68k::big_word;
using v68k::big_longword;

using tap::ok_if;


static void mulu()
{
	using namespace v68k;
	
	uint8_t mem[ 4096 ];
	
	memset( mem, 0xFF, sizeof mem );  // spike memory with bad addresses
	
	uint32_t* vectors = (uint32_t*) mem;
	
	vectors[0] = big_longword( 4096 );  // isp
	vectors[1] = big_longword( 1024 );  // pc
	
	uint16_t* code = (uint16_t*) (mem + 1024);
	
	code[ 0 ] = big_word( 0xC2C0 );  // MULU.W  D0,D1
	code[ 1 ] = big_word( 0xC2C0 );  // MULU.W  D0,D1
	code[ 2 ] = big_word( 0xC2C0 );  // MULU.W  D0,D1
	
	emulator emu( mc68000, mem, sizeof mem );
	
	emu.reset();
	
	emu.regs.nzvc = 0;
	
	emu.regs.d[0] = 0x0001FFFF;
	emu.regs.d[1] = 0x00010000;
	
	
	emu.step();
	
	ok_if( emu.regs.d[1] == 0x00000000 );
	
	ok_if( emu.regs.nzvc == 0x4 );
	
	emu.regs.d[1] = 0x00000001;
	
	
	emu.step();
	
	ok_if( emu.regs.d[1] == 0x0000FFFF );
	
	ok_if( emu.regs.nzvc == 0x0 );
	
	
	emu.step();
	
	ok_if( emu.regs.d[1] == 0xFFFE0001 );
	
	ok_if( emu.regs.nzvc == 0x8 );
}

static void muls()
{
	using namespace v68k;
	
	uint8_t mem[ 4096 ];
	
	memset( mem, 0xFF, sizeof mem );  // spike memory with bad addresses
	
	uint32_t* vectors = (uint32_t*) mem;
	
	vectors[0] = big_longword( 4096 );  // isp
	vectors[1] = big_longword( 1024 );  // pc
	
	uint16_t* code = (uint16_t*) (mem + 1024);
	
	code[ 0 ] = big_word( 0xC3C0 );  // MULS.W  D0,D1
	code[ 1 ] = big_word( 0xC3C0 );  // MULS.W  D0,D1
	code[ 2 ] = big_word( 0xC3C0 );  // MULS.W  D0,D1
	
	emulator emu( mc68000, mem, sizeof mem );
	
	emu.reset();
	
	emu.regs.nzvc = 0;
	
	emu.regs.d[0] = 0x0001FFFF;
	emu.regs.d[1] = 0x00010000;
	
	
	emu.step();
	
	ok_if( emu.regs.d[1] == 0x00000000 );
	
	ok_if( emu.regs.nzvc == 0x4 );
	
	emu.regs.d[1] = 0x00000001;
	
	
	emu.step();
	
	ok_if( emu.regs.d[1] == 0xFFFFFFFF );
	
	ok_if( emu.regs.nzvc == 0x8 );
	
	
	emu.step();
	
	ok_if( emu.regs.d[1] == 0x00000001 );
	
	ok_if( emu.regs.nzvc == 0x0 );
}

int main( int argc, char** argv )
{
	tap::start( "v68k-mul", n_tests );
	
	mulu();
	
	muls();
	
	return 0;
}


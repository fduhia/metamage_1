/*
	fetcher.hh
	----------
*/

#ifndef V68K_FETCHER_HH
#define V68K_FETCHER_HH

// C99
#include <stdint.h>


namespace v68k
{
	
	struct processor_state;
	struct op_params;
	
	
	typedef void (*fetcher)( processor_state& state, op_params& pb );
	
}

#endif


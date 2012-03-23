/*
	memory.hh
	---------
*/

#ifndef MEMORY_HH
#define MEMORY_HH

// v68k-callbacks
#include "callback/memory.hh"


class memory_manager : public v68k::memory
{
	private:
		uint32_t                 its_low_mem_size;
		v68k::low_memory_region  its_low_mem;
		
		v68k::callback::memory  its_callback_memory;
	
	public:
		memory_manager( uint8_t*  low_mem_base,
		                uint32_t  low_mem_size );
		
		uint8_t* translate( uint32_t               addr,
		                    uint32_t               length,
		                    v68k::function_code_t  fc,
		                    v68k::memory_access_t  access ) const;
};

#endif

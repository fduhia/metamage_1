/*
	ProcessList.cc
	--------------
*/

#include "Genie/ProcessList.hh"

// Standard C++
#include <vector>

// Standard C
#include <signal.h>

// poseven
#include "poseven/types/errno_t.hh"

// Nitrogen
#include "Nitrogen/Threads.hh"
#include "Nitrogen/Timer.hh"


namespace Genie
{
	
	namespace n = nucleus;
	namespace N = Nitrogen;
	namespace p7 = poseven;
	
	
	typedef std::vector< boost::intrusive_ptr< Process > > Process_Table;
	
	static Process_Table global_processes;
	
	
	const size_t max_n_tasks = 1024;
	
	static pid_t global_last_pid = 0;
	
	static pid_t next_pid()
	{
		if ( ++global_last_pid >= global_processes.size() )
		{
			global_last_pid = 1;
		}
		
		pid_t last = global_last_pid - 1;
		
		Process_Table::iterator begin = global_processes.begin();
		Process_Table::iterator end   = global_processes.end();
		
		Process_Table::iterator it = std::find( begin + global_last_pid,
		                                        end,
		                                        Process_Table::value_type() );
		
		if ( it == end )
		{
			it = std::find( begin,
			                begin + global_last_pid,
			                Process_Table::value_type() );
			
			if ( it == begin + global_last_pid )
			{
				p7::throw_errno( EAGAIN );
			}
		}
		
		return global_last_pid = it - begin;
	}
	
	
	static bool reaper_must_run = false;
	
	void notify_reaper()
	{
		reaper_must_run = true;
	}
	
	static void* reap_process( void*, pid_t pid, Process& process )
	{
		if ( process.GetLifeStage() == kProcessReleased )
		{
			boost::intrusive_ptr< Process >().swap( global_processes[ pid ] );
		}
		
		return NULL;
	}
	
	namespace
	{
		
		void ReaperThreadEntry()
		{
			while ( true )
			{
				if ( reaper_must_run )
				{
					reaper_must_run = false;
					
					for_each_process( &reap_process );
				}
				
				N::YieldToAnyThread();
			}
		}
		
	}
	
	static const boost::intrusive_ptr< Process >& NewProcess( Process::RootProcess )
	{
		static n::owned< N::ThreadID > reaper = N::NewThread< ReaperThreadEntry >( N::kCooperativeThread );
		
		ASSERT( global_processes.empty() );
		
		global_processes.resize( max_n_tasks );
		
		const pid_t pid = next_pid();
		
		boost::intrusive_ptr< Process > process( new Process( Process::RootProcess() ) );
		
		return global_processes[ pid ] = process;
	}
	
	const boost::intrusive_ptr< Process >& NewProcess( Process& parent )
	{
		const pid_t pid = next_pid();
		
		boost::intrusive_ptr< Process > process( new Process( parent, pid ) );
		
		return global_processes[ pid ] = process;
	}
	
	Process& GetInitProcess()
	{
		static const boost::intrusive_ptr< Process >& init = NewProcess( Process::RootProcess() );
		
		return *init;
	}
	
	static void* kill_process( void*, pid_t, Process& process )
	{
		process.Raise( SIGKILL );
		
		return NULL;
	}
	
	void kill_all_processes()
	{
		Process_Table().swap( global_processes );
	}
	
	
	bool process_exists( pid_t pid )
	{
		return pid < global_processes.size()  &&  global_processes[ pid ].get();
	}
	
	void* for_each_process( void* (*f)( void*, pid_t, Process& ), void* param )
	{
		Process_Table::iterator begin = global_processes.begin();
		Process_Table::iterator end   = global_processes.end();
		
		for ( Process_Table::iterator it = begin;  it != end;  ++it )
		{
			if ( Process* process = it->get() )
			{
				if ( void* result = f( param, it - begin, *process ) )
				{
					return result;
				}
			}
		}
		
		return NULL;
	}
	
	void spawn_process( const char* program_path )
	{
		Process& parent = GetInitProcess();
		
		const boost::intrusive_ptr< Process >& external = NewProcess( parent );
		
		char const *const argv[] = { program_path, NULL };
		
		try
		{
			(void) external->Exec( program_path, argv, NULL );
		}
		catch ( ... )
		{
			global_processes.at( external->GetPID() ).reset();
		}
		
		parent.ResumeAfterFork();
	}
	
}

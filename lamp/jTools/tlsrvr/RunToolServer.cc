/*	================
 *	RunToolServer.cc
 *	================
 */

#include "RunToolServer.hh"

// Mac OS X
#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

// Mac OS
#ifndef __AEREGISTRY__
#include <AERegistry.h>
#endif

// Standard C++
#include <algorithm>

// Standard C/C++
#include <cstdio>

// Iota
#include "iota/strings.hh"

// plus
#include "plus/mac_utf8.hh"
#include "plus/pointer_to_function.hh"
#include "plus/var_string.hh"
#include "plus/string/concat.hh"

// poseven
#include "poseven/extras/slurp.hh"
#include "poseven/extras/spew.hh"
#include "poseven/functions/open.hh"
#include "poseven/functions/stat.hh"
#include "poseven/functions/write.hh"
#include "poseven/types/exit_t.hh"

// mac_pathname
#include "mac_pathname_from_path.hh"

// Nitrogen
#include "Nitrogen/AEDataModel.hh"
#include "Nitrogen/AEInteraction.hh"
#include "Nitrogen/Files.hh"
#include "Nitrogen/Processes.hh"

// FindProcess
#include "FindProcess.hh"

// Divergence
#include "Divergence/Utilities.hh"

// tlsrvr
#include "ToolServer.hh"


namespace tool
{
	
	namespace n = nucleus;
	namespace N = Nitrogen;
	namespace p7 = poseven;
	namespace Div = Divergence;
	namespace NX = NitrogenExtras;
	
	
	static plus::string q( const plus::string& str )
	{
		return "'" + str + "'";
	}
	
	static nucleus::mutable_string& operator<<( nucleus::mutable_string& str, const plus::string& appendage )
	{
		if ( appendage.size() > 0 )
		{
			if ( str.size() > 0 )
			{
				str += " ";
			}
			
			str.append( appendage.data(), appendage.size() );
		}
		
		return str;
	}
	
	
	static plus::string DirectoryCommandForMPW()
	{
		try
		{
			plus::var_string directory_cmd = "Directory '";
			
			directory_cmd += mac_pathname_from_path( "." );
			
			directory_cmd += "'" "\r";
			
			return directory_cmd;
		}
		catch ( ... )
		{
			return "Echo 'Warning: Can't set non-Mac directory as default'\r";
		}
	}
	
	static plus::string make_script_from_command( const plus::string& command )
	{
		plus::var_string script;
		
		script += DirectoryCommandForMPW();
		script += command;
		script += "\r";
		script += "Set CommandStatus {Status}" "\r";
		script += "Directory \"{MPW}\"" "\r";  // don't keep the cwd busy
		script += "Exit {CommandStatus}" "\r";
		
		return script;
	}
	
	static nucleus::string MakeToolServerScript( const char*  script_path,
	                                             const char*  out_path,
	                                             const char*  err_path )
	{
		nucleus::mutable_string script;
		
		script << "Set Exit 0;";
		
		script << q( mac_pathname_from_path( script_path ) );
		script << "< Dev:Null";
		
		plus::string outPath = mac_pathname_from_path( out_path );
		plus::string errPath = mac_pathname_from_path( err_path );
		// FIXME:  This is case-sensitive
		//bool identicalOutputAndError = outPath == errPath;
		bool identicalOutputAndError = false;
		if ( identicalOutputAndError )
		{
			script << "\xB7" << q( outPath );  // sum symbol
		}
		else
		{
			script << ">"    << q( outPath );
			script << "\xB3" << q( errPath );  // greater-than-or-equal-to
		}
		
		return script;
	}
	
	
	static inline int device_of_ramdisk()
	{
		if ( TARGET_API_MAC_CARBON )
		{
			// MacRelix Carbon doesn't check driver names, and
			// OS X's BSD layer doesn't have /sys in the first place
			return 0;
		}
		
		struct stat ram_status = { 0 };
		
		const bool has_ramdisk = p7::stat( "/sys/mac/vol/ram/mnt", ram_status );
		
		return has_ramdisk ? ram_status.st_dev : 0;
	}
	
	static ProcessSerialNumber launch_ToolServer_from_ramdisk( int dev )
	{
		const N::FSVolumeRefNum vRefNum = N::FSVolumeRefNum( -dev );
		
		return N::LaunchApplication( N::DTGetAPPL( sigToolServer, vRefNum ) );
	}
	
	
	static long GetResult( const Mac::AppleEvent& reply )
	{
		SInt32 stat = N::AEGetParamPtr< Mac::typeSInt32 >( reply, Mac::AEKeyword( 'stat' ) );
		
		if ( stat == -1 )
		{
			stat = 127;
		}
		
		return stat;
	}
	
	
	static ProcessSerialNumber find_or_launch_ToolServer()
	{
		try
		{
			return NX::FindProcess( sigToolServer );
		}
		catch ( const Mac::OSStatus& err )
		{
			if ( err != procNotFound )
			{
				throw;
			}
		}
		
		if ( const int device = device_of_ramdisk() )
		{
			try
			{
				return launch_ToolServer_from_ramdisk( device );
			}
			catch ( const Mac::OSStatus& err )
			{
				if ( err != afpItemNotFound )
				{
					throw;
				}
			}
		}
		
		try
		{
			return N::LaunchApplication( N::DTGetAPPL( sigToolServer ) );
		}
		catch ( const Mac::OSStatus& err )
		{
			if ( err == afpItemNotFound )
			{
				p7::write( p7::stderr_fileno, STR_LEN( "tlsrvr: ToolServer not found\n" ) );
			}
			else if ( TARGET_API_MAC_CARBON  &&  err == -10661 )
			{
				p7::write( p7::stderr_fileno, STR_LEN( "tlsrvr: ToolServer not runnable on this system\n" ) );
			}
			else
			{
				throw;
			}
		}
		
		throw p7::exit_failure;
	}
	
	static n::owned< Mac::AppleEvent > CreateScriptEvent( const ProcessSerialNumber&  psn,
	                                                      const nucleus::string&      script )
	{
		n::owned< Mac::AppleEvent > appleEvent = N::AECreateAppleEvent( Mac::kAEMiscStandards,
		                                                                Mac::kAEDoScript,
		                                                                N::AECreateDesc< Mac::typeProcessSerialNumber >( psn ) );
		
		N::AEPutParamDesc( appleEvent, Mac::keyDirectObject, N::AECreateDesc< Mac::typeChar >( script ) );
		
		return appleEvent;
	}
	
	static n::owned< Mac::AppleEvent > AESendBlocking( const Mac::AppleEvent& appleEvent )
	{
		n::owned< Mac::AppleEvent > replyEvent = N::AEInitializeDesc< Mac::AppleEvent >();
		
		// Declare a block to limit the scope of mutableReply
		{
			N::Detail::AEDescEditor< Mac::AppleEvent > mutableReply( replyEvent );
			
			Mac::ThrowOSStatus( Div::AESendBlocking( &appleEvent, &mutableReply.Get() ) );
			
			// Reply is available.  End scope to restore the reply.
		}
		
		return replyEvent;
	}
	
	enum
	{
		kScriptFile,
		kOutputFile,
		kErrorFile
	};
	
	static void make_temp_file( const char* path )
	{
		(void) p7::open( path,
		                 p7::o_wronly | p7::o_creat | p7::o_trunc,
		                 p7::_666 );
	}
	
	const int n_files = 3;
	
	static char const* temp_file_paths[ n_files ] =
	{
		"/tmp/.tlsrvr-" "script", 
		"/tmp/.tlsrvr-" "stdout", 
		"/tmp/.tlsrvr-" "stderr"
	};
	
	static nucleus::string SetUpScript( const plus::string& command )
	{
		// Send a Do Script event with the command as the direct object.
		// Better yet:
		//  * Write the command to a file (which we'll invoke by its filename)
		// so we don't have to quote the command.
		//  * Create temp files to store I/O.
		//  * Run the script with I/O redirected.
		//  * Dump the stored output to stdout and stderr.
		
		// It's okay if command has the fancy quotes, because we don't actually
		// refer to it in the Apple event itself.
		
		std::for_each( temp_file_paths,
		               temp_file_paths + n_files,
		               plus::ptr_fun( make_temp_file ) );
		
		plus::string inner_script = make_script_from_command( command );
		
		p7::spew( p7::open( temp_file_paths[ kScriptFile ], p7::o_wronly ),
		          inner_script.data(),
		          inner_script.size() );
		
		nucleus::string script = MakeToolServerScript( temp_file_paths[ kScriptFile ],
		                                               temp_file_paths[ kOutputFile ],
		                                               temp_file_paths[ kErrorFile  ] );
		
		return script;
	}
	
	static void ConvertAndDumpMacText( plus::var_string& text, p7::fd_t fd )
	{
		std::replace( text.begin(), text.end(), '\r', '\n' );
		
		p7::write( fd, plus::utf8_from_mac( text ) );
	}
	
	static void dump_file( const char* path, p7::fd_t fd )
	{
		plus::var_string text = p7::slurp( path );
		
		ConvertAndDumpMacText( text, fd );
	}
	
	static inline bool matches_at_end( const char* a_end,
	                                   size_t a_length,
	                                   const char* b_begin,
	                                   size_t b_length )
	{
		const char* a_begin = a_end - b_length;
		
		return a_length >= b_length  &&  std::equal( a_begin, a_end, b_begin );
	}
	
	static bool find_from_end( const char* begin,
	                           const char* end,
	                           const char* pattern,
	                           size_t length )
	{
		const char* pattern_end = pattern + length;
		
		for ( const char* p = end - length;  p >= begin;  --p )
		{
			if ( std::equal( pattern, pattern_end, p ) )
			{
				return true;
			}
		}
		
		return false;
	}
	
	static bool strings_equal( const char* a_begin,
	                           const char* a_end,
	                           const char* b_begin,
	                           size_t b_length )
	{
		const size_t a_length = a_end - a_begin;
		
		return a_length == b_length  &&  std::equal( a_begin, a_end, b_begin );
	}
	
	static bool user_cancelled( const plus::string& errors )
	{
		const char* begin = &*errors.begin();
		const char* end   = &*errors.end  ();
		
		if ( strings_equal( begin,
		                    end,
		                    STR_LEN( "\r" "User break, cancelled...\r" ) ) )
		{
			return true;
		}
		
		// The magic line we're looking for is:
		//
		//     ### ToolServer - Execution of HD:Path:To:Script terminated.
		
		const bool terminated = matches_at_end( end,
		                                        errors.size(),
		                                        STR_LEN( " terminated.\r" ) );
		
		if ( terminated )
		{
			return find_from_end( begin,
			                      end,
			                      STR_LEN( "### ToolServer - Execution of" ) );
		}
		
		return false;
	}
	
	static void switch_process( const ProcessSerialNumber& from,
	                            const ProcessSerialNumber& to )
	{
		if ( N::SameProcess( from, N::GetFrontProcess() ) )
		{
			N::SetFrontProcess( to );
			
			while ( N::SameProcess( from, N::GetFrontProcess() ) )
			{
				sleep( 0 );
			}
		}
	}
	
	int RunCommandInToolServer( const plus::string& command, bool switch_layers )
	{
		const ProcessSerialNumber toolServer = find_or_launch_ToolServer();
		
		// This is a bit of a hack.
		// It really ought to happen just after we send the event.
		if ( switch_layers )
		{
			switch_process( N::CurrentProcess(), toolServer );
		}
		
		int result = GetResult( AESendBlocking( CreateScriptEvent( toolServer,
		                                                           SetUpScript( command ) ) ) );
		
		if ( switch_layers )
		{
			switch_process( toolServer, N::CurrentProcess() );
		}
		
		if ( result == -9 )
		{
			// Observed from MWLink68K and MWLinkPPC on Command-period
			return 128;
		}
		
		plus::var_string errors = p7::slurp( temp_file_paths[ kErrorFile ] );
		
		// A Metrowerks tool returns 1 on error and 2 on user break, except that
		// if you limit the number of diagnostics displayed and there more errors
		// than the limit, it pretends that YOU cancelled it while printing the
		// output, claiming "User break", and returning 2.
		// Also, sometimes 1 is returned on user cancel, which could mean that
		// ToolServer was running a script rather than a Metrowerks tool.
		// And canceling a script might also return 0.  Go figure.
		
		if ( result <= 2 )
		{
			if ( user_cancelled( errors ) )
			{
				// User pressed Command-period
				return 128;
			}
			else if ( result == 2 )
			{
				// Sorry, but the existence of more errors than I asked to be
				// printed does NOT equal user-sponsored cancellation.
				result = 1;
			}
		}
		
		ConvertAndDumpMacText( errors, p7::stderr_fileno );
		
		dump_file( temp_file_paths[ kOutputFile ], p7::stdout_fileno );
		
		// Delete temp files
		std::for_each( temp_file_paths,
		               temp_file_paths + n_files,
		               plus::ptr_fun( ::unlink ) );
		
		return result;
	}
	
}


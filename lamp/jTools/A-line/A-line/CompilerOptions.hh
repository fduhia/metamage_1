/*	==================
 *	CompilerOptions.hh
 *	==================
 */

#ifndef ALINE_COMPILEROPTIONS_HH
#define ALINE_COMPILEROPTIONS_HH

// C++
#include <list>
#include <vector>

// plus
#include "plus/string.hh"

// A-line
#include "A-line/TargetTypes.hh"


namespace tool
{
	
	typedef std::vector< const char* > Macros;
	
	struct TargetInfo;
	
	
	class CompilerOptions
	{
		private:
			plus::string                 its_project_name;
			const TargetInfo&            its_target_info;
			Macros                       its_macros;
			std::list< plus::string >    its_macro_storage;
			std::vector< plus::string >  its_include_search_options;
			plus::string                 its_precompiled_header_source_pathname;
			bool                         it_has_precompiled_header_source;
		
		public:
			CompilerOptions( const plus::string&  name,
			                 const TargetInfo&    its_target_info );
			
			const plus::string& Name() const  { return its_project_name; }
			const TargetInfo& Target() const  { return its_target_info; }
			
			void AddDefinedMacro( const char* macro_definition );
			
			void DefineMacro( const plus::string& macro, const plus::string& value );
			void DefineMacro( const plus::string& macro, bool value = true );
			
			void SetPrecompiledHeaderSource( const plus::string& pch );
			void SetOutput( const plus::string& output );
			
			const Macros& GetMacros() const  { return its_macros; }
			
			bool HasPrecompiledHeaderSource() const  { return it_has_precompiled_header_source; }
			
			const plus::string& PrecompiledHeaderSource() const  { return its_precompiled_header_source_pathname; }
			
			std::vector< plus::string > const& IncludeDirOptions() const  { return its_include_search_options; }
			
			void AppendIncludeDir ( const plus::string& dir );
			void PrependIncludeDir( const plus::string& dir );
			
	};
	
}

#endif


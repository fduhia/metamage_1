/*	=====================
 *	FSTree_sys_mac_crm.hh
 *	=====================
 */

#ifndef GENIE_FILESYSTEM_FSTREESYSMACCRM_HH
#define GENIE_FILESYSTEM_FSTREESYSMACCRM_HH

// Genie
#include "Genie/FileSystem/FSTree_Directory.hh"


namespace Genie
{
	
	class FSTree_sys_mac_crm : public FSTree_Functional_Singleton
	{
		public:
			FSTree_sys_mac_crm( const FSTreePtr& parent ) : FSTree_Functional_Singleton( parent )
			{
			}
			
			void Init();
			
			static std::string OnlyName()  { return "crm"; }
			
			std::string Name() const  { return OnlyName(); }
	};
	
}

#endif


// AEDataModel.h

#ifndef CLASSICTOOLBOX_AEDATAMODEL_H
#define CLASSICTOOLBOX_AEDATAMODEL_H

#ifndef __EPPC__
#include <EPPC.h>
#endif

#ifndef NITROGEN_AEDATAMODEL_H
#include "Nitrogen/AEDataModel.h"
#endif


namespace Nitrogen
{
	
	// Deprecated addressing modes under Carbon
	static const DescType typeSessionID    = DescType( ::typeSessionID    );
	static const DescType typeTargetID     = DescType( ::typeTargetID     );
	static const DescType typeDispatcherID = DescType( ::typeDispatcherID );
	
	// TargetID is defined for Carbon, but typeTargetID is not.
	
	template <> struct DescType_Traits< typeTargetID > : Nucleus::PodFlattener< TargetID > {};
	
}

#endif


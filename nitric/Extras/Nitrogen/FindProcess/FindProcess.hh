/*
	FindProcess.hh
	--------------
	
	Copyright 2009, Joshua Juran
*/

#ifndef FINDPROCESS_HH
#define FINDPROCESS_HH

// Mac OS
#ifndef __PROCESSES__
#include <Processes.h>
#endif

// Nitrogen
#include "Mac/Files/Types/FSCreator.hh"


namespace NitrogenExtras
{
	
	ProcessSerialNumber FindProcess( Mac::FSCreator signature );
	
	ProcessSerialNumber FindProcess( const FSSpec& appFile );
	
}

#endif


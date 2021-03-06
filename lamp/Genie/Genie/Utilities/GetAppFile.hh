/*
	GetAppFile.hh
	-------------
	
	Copyright 2009, Joshua Juran
*/

#ifndef GENIE_UTILITIES_GETAPPFILE_HH
#define GENIE_UTILITIES_GETAPPFILE_HH

// Mac OS X
#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif

// Mac OS
#ifndef __FILES__
#include <Files.h>
#endif


namespace Genie
{
	
	FSSpec GetAppFile();
	
}

#endif


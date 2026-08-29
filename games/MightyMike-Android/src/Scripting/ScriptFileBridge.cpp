#ifdef PANGEA_ENABLE_SCRIPTING

#include "Pomme.h"
#include "PommeFiles.h"
#include "CompilerSupport/filesystem.h"

#include <string>

extern "C" short MightyMikeScript_OpenDataFile(const char* file)
{
	try
	{
		if (!file || std::string(file).compare(0, 9, ":Scripts:") != 0)
			return -1;
		std::string relative(file + 9);
		for (char& character : relative)
			if (character == ':') character = '/';
		FSSpec spec = Pomme::Files::HostPathToFSSpec(fs::path("Data") / "Scripts" / relative);
		short refNum = -1;
		if (FSpOpenDF(&spec, fsRdPerm, &refNum) != noErr)
			return -1;
		return refNum;
	}
	catch (...)
	{
		return -1;
	}
}

#endif

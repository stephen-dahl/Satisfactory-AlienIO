// Copyright Epic Games, Inc. All Rights Reserved.

#include "AlienIO.h"

#define LOCTEXT_NAMESPACE "FAlienIOModule"
DEFINE_LOG_CATEGORY(LogAlienIO);


void FAlienIOModule::StartupModule()
{
	UE_LOG(LogAlienIO, Verbose, TEXT("AlienIO has started!"));
}

void FAlienIOModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAlienIOModule, AlienIO)
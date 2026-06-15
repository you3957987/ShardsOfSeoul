#pragma once

#include "Modules/ModuleManager.h"

class FEnemyModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

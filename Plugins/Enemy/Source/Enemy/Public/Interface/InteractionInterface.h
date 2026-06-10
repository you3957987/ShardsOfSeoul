#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractionInterface.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnStateChanged, bool, bShouldLockOn);

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

class ENEMY_API IInteractionInterface
{
	GENERATED_BODY()

public:
	
	virtual FOnLockOnStateChanged& GetLockOnStateChangedDelegate() = 0;
	
};
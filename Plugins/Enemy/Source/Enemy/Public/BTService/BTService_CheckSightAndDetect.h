#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_CheckSightAndDetect.generated.h"


UCLASS()
class ENEMY_API UBTService_CheckSightAndDetect : public UBTService_BlackboardBase
{
	GENERATED_BODY()
	
public:
	UBTService_CheckSightAndDetect();
	
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	
};

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_HechiTeleport.generated.h"

UCLASS()
class ENEMY_API UBTTask_HechiTeleport : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
public:
	UBTTask_HechiTeleport();
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

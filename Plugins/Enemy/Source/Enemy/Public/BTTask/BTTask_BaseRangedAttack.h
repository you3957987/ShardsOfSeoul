#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_BaseRangedAttack.generated.h"


UCLASS()
class ENEMY_API UBTTask_BaseRangedAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_BaseRangedAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
};

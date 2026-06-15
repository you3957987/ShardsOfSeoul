#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_BaseTransparAttack.generated.h"


UCLASS()
class ENEMY_API UBTTask_BaseTransparAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_BaseTransparAttack();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

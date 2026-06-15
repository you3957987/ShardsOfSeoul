#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_BaseMimicAttack.generated.h"


UCLASS()
class ENEMY_API UBTTask_BaseMimicAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_BaseMimicAttack();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

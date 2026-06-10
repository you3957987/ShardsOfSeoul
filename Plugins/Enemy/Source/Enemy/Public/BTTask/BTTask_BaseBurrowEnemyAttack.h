#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_BaseBurrowEnemyAttack.generated.h"


UCLASS()
class ENEMY_API UBTTask_BaseBurrowEnemyAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_BaseBurrowEnemyAttack();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

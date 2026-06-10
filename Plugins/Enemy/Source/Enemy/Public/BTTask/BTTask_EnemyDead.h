#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_EnemyDead.generated.h"


UCLASS()
class ENEMY_API UBTTask_EnemyDead : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
public:
	UBTTask_EnemyDead();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
};

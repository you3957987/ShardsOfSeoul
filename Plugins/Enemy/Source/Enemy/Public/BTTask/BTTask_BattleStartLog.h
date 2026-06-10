#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_BattleStartLog.generated.h"


UCLASS()
class ENEMY_API UBTTask_BattleStartLog : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
public:
	UBTTask_BattleStartLog();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

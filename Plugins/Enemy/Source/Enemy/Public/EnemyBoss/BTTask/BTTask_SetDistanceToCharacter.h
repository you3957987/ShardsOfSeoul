#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_SetDistanceToCharacter.generated.h"


UCLASS()
class ENEMY_API UBTTask_SetDistanceToCharacter : public UBTTask_BlackboardBase
{
	GENERATED_BODY()


public:
	UBTTask_SetDistanceToCharacter();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
	UPROPERTY(EditAnywhere, Category = Blackboard)
	FBlackboardKeySelector TargetKey; // 타겟 (Actor)

	UPROPERTY(EditAnywhere, Category = Blackboard)
	FBlackboardKeySelector DistanceKey; // 저장할 거리 (Float)
};

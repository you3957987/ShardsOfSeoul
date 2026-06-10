#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_BlackKnightGuard.generated.h"

UCLASS()
class ENEMY_API UBTTask_BlackKnightGuard : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
	float CurrentTime = 0.0f;

	float GuardDuration = 3.0f;
	
protected:
	// 태스크 시작 시 실행
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    
	// 태스크가 InProgress 상태일 때 매 프레임 실행
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	
public:
	UBTTask_BlackKnightGuard();
	
	
};

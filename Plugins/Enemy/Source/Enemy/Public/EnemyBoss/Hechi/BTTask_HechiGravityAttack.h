#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_HechiGravityAttack.generated.h"


UCLASS()
class ENEMY_API UBTTask_HechiGravityAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
	bool bIsAttacking = false;
	
	UAnimMontage* CurrentAttackMontage;
	
public:
	UBTTask_HechiGravityAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	// 틱 태스크
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

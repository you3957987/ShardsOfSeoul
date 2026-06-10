#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MagicSwordManCloseAttack.generated.h"

// 깃 추가 확인용
UCLASS()
class ENEMY_API UBTTask_MagicSwordManCloseAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
	bool bIsAttacking = false;
	
	UAnimMontage* CurrentAttackMontage;
	
public:
	UBTTask_MagicSwordManCloseAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	// 틱 태스크
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

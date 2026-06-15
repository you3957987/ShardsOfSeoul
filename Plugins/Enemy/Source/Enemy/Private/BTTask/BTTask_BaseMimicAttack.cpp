#include "BTTask/BTTask_BaseMimicAttack.h"

#include "AIController.h"
#include "BaseEnemy.h"

class ABaseEnemy;

UBTTask_BaseMimicAttack::UBTTask_BaseMimicAttack()
{
	NodeName = "BaseMimicAttack"; 
}

EBTNodeResult::Type UBTTask_BaseMimicAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	if ( OwnerComp.GetAIOwner() == nullptr ) return EBTNodeResult::Failed;

	ABaseEnemy* Enemy = Cast<ABaseEnemy>( OwnerComp.GetAIOwner()->GetPawn() );
	if ( Enemy == nullptr ) return EBTNodeResult::Failed;

	Enemy->Attack();// 적의 공격 함수 호출
	
	return EBTNodeResult::Succeeded;
}

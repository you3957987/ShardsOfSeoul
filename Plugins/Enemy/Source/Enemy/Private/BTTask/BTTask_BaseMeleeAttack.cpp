#include "BTTask/BTTask_BaseMeleeAttack.h"
#include "AIController.h"
#include "BaseEnemy.h"

UBTTask_BaseMeleeAttack::UBTTask_BaseMeleeAttack()
{
	NodeName = "BaseMeleeAttack"; 
}

EBTNodeResult::Type UBTTask_BaseMeleeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	if ( OwnerComp.GetAIOwner() == nullptr ) return EBTNodeResult::Failed;

	ABaseEnemy* Enemy = Cast<ABaseEnemy>( OwnerComp.GetAIOwner()->GetPawn() );
	if ( Enemy == nullptr ) return EBTNodeResult::Failed;

	Enemy->Attack();// 적의 공격 함수 호출
	
	return EBTNodeResult::Succeeded;
}


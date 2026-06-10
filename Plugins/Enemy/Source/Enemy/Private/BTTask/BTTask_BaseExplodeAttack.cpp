#include "BTTask/BTTask_BaseExplodeAttack.h"
#include "AIController.h"
#include "BaseEnemy.h"

UBTTask_BaseExplodeAttack::UBTTask_BaseExplodeAttack()
{
	NodeName = TEXT("Explode Attack");
}

EBTNodeResult::Type UBTTask_BaseExplodeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	if ( OwnerComp.GetAIOwner() == nullptr ) return EBTNodeResult::Failed;

	ABaseEnemy* Enemy = Cast<ABaseEnemy>( OwnerComp.GetAIOwner()->GetPawn() );
	if ( Enemy == nullptr ) return EBTNodeResult::Failed;

	Enemy->Attack();// 적의 공격 함수 호출
	
	return EBTNodeResult::Succeeded;
}

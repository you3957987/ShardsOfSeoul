#include "BTTask/BTTask_Guard_GuardReaction.h"

#include "AIController.h"
#include "Enemy/BaseGuardEnemy.h"

UBTTask_Guard_GuardReaction::UBTTask_Guard_GuardReaction()
{
	NodeName = "GuardReactionAttack"; 
}

EBTNodeResult::Type UBTTask_Guard_GuardReaction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	if ( OwnerComp.GetAIOwner() == nullptr ) return EBTNodeResult::Failed;

	ABaseGuardEnemy* Enemy = Cast<ABaseGuardEnemy>( OwnerComp.GetAIOwner()->GetPawn() );
	if ( Enemy == nullptr ) return EBTNodeResult::Failed;

	Enemy->GuardReactionAttack();// 적의 공격 함수 호출
	
	return EBTNodeResult::Succeeded;
}


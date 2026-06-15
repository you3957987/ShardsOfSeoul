#include "BTTask/BTTask_BaseBurrowEnemyAttack.h"

#include "AIController.h"
#include "Enemy/BaseBurrowEnemy.h"

UBTTask_BaseBurrowEnemyAttack::UBTTask_BaseBurrowEnemyAttack()
{
	NodeName = TEXT("Burrow Enemy Attack");
}

EBTNodeResult::Type UBTTask_BaseBurrowEnemyAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	if ( OwnerComp.GetAIOwner() == nullptr ) return EBTNodeResult::Failed;

	ABaseBurrowEnemy* Enemy = Cast<ABaseBurrowEnemy>( OwnerComp.GetAIOwner()->GetPawn() );
	if ( Enemy == nullptr ) return EBTNodeResult::Failed;

	if ( Enemy->bIsBurrowing ) return EBTNodeResult::Failed; // 땅파는 중이면 공격 못하게 함
	
	
	Enemy->Attack();// 적의 공격 함수 호출
	
	return EBTNodeResult::Succeeded;
}

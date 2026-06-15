#include "BTTask/BTTask_Guard_GuardPattern.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/BaseGuardEnemy.h"

UBTTask_Guard_GuardPattern::UBTTask_Guard_GuardPattern()
{
	NodeName = "Guard_Guard Pattern";
	// TickTask를 사용하려면 반드시 true로 설정해야 함
	bNotifyTick = true; 
}

EBTNodeResult::Type UBTTask_Guard_GuardPattern::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard) return EBTNodeResult::Failed;

	// 1. 초기화: 시간 0, 피격 횟수 0, 가드 상태 True
	CurrentTime = 0.0f;

	// 가드 리엑션 보드 초기화
	Blackboard->SetValueAsBool("GuardAttack", false);

	AAIController* AIController = OwnerComp.GetAIOwner();
	ABaseGuardEnemy* Enemy = Cast<ABaseGuardEnemy>(AIController->GetPawn());
	if ( Enemy )
	{
		GuardDuration = Enemy->GuardDuration;
		Enemy->bIsGuarding = true;
		Enemy->DamageWhileGuarding = 0.0f;
	}
	
	// 2. 중요: 즉시 끝내지 않고 '진행 중' 상태를 반환
	return EBTNodeResult::InProgress;
}

void UBTTask_Guard_GuardPattern::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	ABaseGuardEnemy* Enemy = Cast<ABaseGuardEnemy>(AIController->GetPawn());

	// [추가] 적이 없으면 그냥 실패 처리하고 리턴 (안전장치)
	if (!Enemy)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	
	CurrentTime += DeltaSeconds;
	
	if (CurrentTime >= GuardDuration)
	{
		// 가드 시간이 다 되었다면 가드 해제
		Blackboard->SetValueAsName(GetSelectedBlackboardKey(), FName("GuardEnd")); // 가드 종료 상태로 전환
		Enemy->bIsGuarding = false;

		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}
	
	if (Enemy->DamageWhileGuarding >= Enemy->MaxDamageToReaction )
	{
		// 피격 횟수가 최대치에 도달했으면 가드 리엑션 실행
		Blackboard->SetValueAsName(GetSelectedBlackboardKey(), FName("GuardReaction")); // 가드 리엑션 상태로 전환
		Enemy->bIsGuarding = false;
		
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);

		return;
	}
}

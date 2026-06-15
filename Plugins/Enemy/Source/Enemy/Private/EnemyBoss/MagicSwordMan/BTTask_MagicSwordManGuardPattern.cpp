#include "EnemyBoss/MagicSwordMan/BTTask_MagicSwordManGuardPattern.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/MagicSwordMan/BossMagicSwordMan.h"

// 깃 추가 확인용
UBTTask_MagicSwordManGuardPattern::UBTTask_MagicSwordManGuardPattern()
{
	NodeName = "MagicSwordMan Guard Pattern";
	// TickTask를 사용하려면 반드시 true로 설정해야 함
	bNotifyTick = true; 
}

EBTNodeResult::Type UBTTask_MagicSwordManGuardPattern::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard) return EBTNodeResult::Failed;

	// 1. 초기화: 시간 0, 피격 횟수 0, 가드 상태 True
	CurrentTime = 0.0f;

	// 가드 리엑션 보드 초기화
	Blackboard->SetValueAsBool("GuardAttack", false);

	AAIController* AIController = OwnerComp.GetAIOwner();
	ABossMagicSwordMan* Boss = Cast<ABossMagicSwordMan>(AIController->GetPawn());
	if ( Boss )
	{
		GuardDuration = Boss->AttackStruct.GuardDuration;
		Boss->bIsGuarding = true;
		Boss->DamageWhileGuarding = 0.0f;
		
		Boss->BossMagicSwordManLogData.GuardCount++;
		
		Blackboard->SetValueAsFloat("AttackDelay", Boss->AttackStruct.GuardDelay); // 행동 딜레이 설정
	}
	
	// 2. 중요: 즉시 끝내지 않고 '진행 중' 상태를 반환
	return EBTNodeResult::InProgress;
}

void UBTTask_MagicSwordManGuardPattern::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	ABossMagicSwordMan* Boss = Cast<ABossMagicSwordMan>(AIController->GetPawn());

	// [추가] 보스가 없으면 그냥 실패 처리하고 리턴 (안전장치)
	if (!Boss)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	
	CurrentTime += DeltaSeconds;
	
	if (CurrentTime >= GuardDuration)
	{
		// 가드 시간이 다 되었다면 가드 해제
		Blackboard->SetValueAsBool("GuardAttack", false);
		Boss->bIsGuarding = false;

		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}
	
	if (Boss->DamageWhileGuarding >= Boss->AttackStruct.MaxDamageToReaction )
	{
		// 피격 횟수가 최대치에 도달했으면 가드 리엑션 실행
		Blackboard->SetValueAsBool("GuardAttack", true);
		Boss->bIsGuarding = false;
		
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);

		return;
	}
}
#include "EnemyBoss/Worm/BTTask_WormSuction.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/Worm/BossWorm.h"

UBTTask_WormSuction::UBTTask_WormSuction()
{
	NodeName = "WormSuction";

	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_WormSuction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ABossWorm* Boss = Cast<ABossWorm>(AIController->GetPawn());

	if (Boss == nullptr)
	{
		return EBTNodeResult::Failed;
	}
	
	// 1. 공격 함수 실행 (몽타주 재생 시작)
	Boss->SuctionStartMontagePlay();
	bIsAttacking = true;

	// 2. 중요: "성공(Succeeded)"을 바로 리턴하면 안 됩니다!
	// 몽타주가 끝날 때까지 기다려야 하므로 "진행 중(InProgress)"을 리턴합니다.
	return EBTNodeResult::InProgress;
}

void UBTTask_WormSuction::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	// 공격 중이 아니면 틱을 돌 필요가 없음
	if (!bIsAttacking) return;

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return;

	ABossWorm* Boss = Cast<ABossWorm>(AIController->GetPawn());
	if (!Boss) return;

	// 흡입 공격 범위 내에 들어오면 즉시 중단 및 성공 처리
	if (Boss->bIsInSuctionAttackArea == true)
	{
		// 몽타주 강제 중단 (블렌드 아웃 시간 0.2초)
		if (UAnimInstance* AnimInstance = Boss->GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.2f, Boss->SuctionMontage);
		}

		bIsAttacking = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		
		return;
	}

	// 기존 로직: 몽타주 재생이 자연스럽게 끝났는지 확인
	if (!Boss->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Boss->SuctionMontage))
	{
		// 3. 몽타주가 끝났으므로 태스크 종료를 선언합니다.
		bIsAttacking = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}


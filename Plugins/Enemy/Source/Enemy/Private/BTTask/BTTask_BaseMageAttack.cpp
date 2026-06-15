#include "BTTask/BTTask_BaseMageAttack.h"

#include "AIController.h"
#include "Enemy/BaseMageEnemy.h"

UBTTask_BaseMageAttack::UBTTask_BaseMageAttack()
{
	NodeName = "BaseMageAttack";

	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_BaseMageAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ABaseMageEnemy* Enemy = Cast<ABaseMageEnemy>(AIController->GetPawn());

	if (Enemy == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	// 1. 공격 함수 실행 (몽타주 재생 시작)
	CurrentAttackMontage = Enemy->Attack();
	bIsAttacking = true;

	// 2. 중요: "성공(Succeeded)"을 바로 리턴하면 안 됩니다!
	// 몽타주가 끝날 때까지 기다려야 하므로 "진행 중(InProgress)"을 리턴합니다.
	return EBTNodeResult::InProgress;
}

void UBTTask_BaseMageAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	// 공격 중이 아니면 틱을 돌 필요가 없음
	if (!bIsAttacking) return;
	
	AAIController* AIController = OwnerComp.GetAIOwner();
	ABaseMageEnemy* Enemy = Cast<ABaseMageEnemy>(AIController->GetPawn());

	// Boss가 유효하고, 몽타주 재생이 끝났는지 확인
	// (Montage_IsPlaying은 지정한 몽타주가 재생 중이면 true, 아니면 false)
	if (Enemy && !Enemy->GetMesh()->GetAnimInstance()->Montage_IsPlaying(CurrentAttackMontage) )
	{
		// 3. 몽타주가 끝났으므로 태스크 종료를 선언합니다.
		bIsAttacking = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}
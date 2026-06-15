#include "EnemyBoss/BTService/BTService_BossFocusPlayer.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/BaseBossEnemy.h"

UBTService_BossFocusPlayer::UBTService_BossFocusPlayer()
{
	NodeName = TEXT("Focus Player When Wait");
}

void UBTService_BossFocusPlayer::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	// 1. AI 컨트롤러와 Pawn(적 캐릭터) 가져오기
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return;

	ABaseBossEnemy* Enemy = Cast<ABaseBossEnemy>(AIController->GetPawn());
	
	// 2. 적 캐릭터가 유효하고, 회전이 허용된 상태(bFocusPlayerAfterAttack)인지 확인
	if (Enemy && Enemy->bFocusPlayerAfterAttack)
	{
		UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
		if (BlackboardComp)
		{
			// 3. 블랙보드에서 플레이어 위치(Vector) 가져오기
			// (에디터 서비스 노드에서 'Blackboard Key'를 PlayerLocation으로 설정해야 함)
			FVector TargetLocation = BlackboardComp->GetValueAsVector(GetSelectedBlackboardKey());

			// 4. 타겟 방향 계산 (Z축 높이 차이는 무시)
			FVector LookDirection = TargetLocation - Enemy->GetActorLocation();
			LookDirection.Z = 0.f;

			if (!LookDirection.IsNearlyZero())
			{
				FRotator TargetRotation = LookDirection.Rotation();
				FRotator CurrentRotation = Enemy->GetActorRotation();

				// 5. 부드럽게 회전 (RInterpTo)
				FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaSeconds, RotationSpeed);
				
				Enemy->SetActorRotation(NewRotation);
			}
		}
	}
}

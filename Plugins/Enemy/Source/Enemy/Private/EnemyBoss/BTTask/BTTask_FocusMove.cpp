#include "EnemyBoss/BTTask/BTTask_FocusMove.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_FocusMove::UBTTask_FocusMove()
{
	NodeName = "FocusMove";
	
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_FocusMove::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return EBTNodeResult::InProgress;
}

void UBTTask_FocusMove::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	
	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* Pawn = AIC->GetPawn();
   
	// 1. 블랙보드에서 목표 위치(EQS)와 타겟 액터 가져오기
	FVector TargetLocation = OwnerComp.GetBlackboardComponent()->GetValueAsVector(TEXT("EQC"));
	AActor* TargetActor = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TEXT("TargetCharacter")));

	if (Pawn && TargetActor)
	{
		// 2. 강제 회전: 무조건 플레이어를 바라보게 함
		FVector LookDir = TargetActor->GetActorLocation() - Pawn->GetActorLocation();
		LookDir.Z = 0.0f;
		Pawn->SetActorRotation(LookDir.Rotation());

		// 3. 방향 벡터 추출 및 평면화
		// 목표 위치를 향하는 방향만 뽑아낸 뒤, 땅을 파고들거나 허공을 걷지 않게 Z축을 날립니다.
		FVector MoveDir = (TargetLocation - Pawn->GetActorLocation()).GetSafeNormal();
		MoveDir.Z = 0.0f;
		MoveDir.Normalize();

		// 디버그: 구체 대신 '나아갈 방향'을 화살표로 그리면 확인하기 훨씬 편합니다.
		//DrawDebugDirectionalArrow(GetWorld(), Pawn->GetActorLocation(), Pawn->GetActorLocation() + (MoveDir * 200.f), 50.f, FColor::Green, false, 0.1f);

		// 4. 해당 방향으로 이동 입력 추가
		Pawn->AddMovementInput(MoveDir, 1.0f);
	}
}

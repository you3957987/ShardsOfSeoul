#include "BTService/BTService_CheckSightAndDetect.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnemyAiController.h"


UBTService_CheckSightAndDetect::UBTService_CheckSightAndDetect()
{
	NodeName = "Check Sight And DetectRange";
}

void UBTService_CheckSightAndDetect::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	APawn* TargetPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (TargetPawn == nullptr) return;

	AEnemyAiController* AIController = Cast<AEnemyAiController>(OwnerComp.GetAIOwner());
	if (AIController == nullptr) return;

	if ( AIController->bAlwaysChase == true ) // 무조건 추적 모드인 경우
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsVector(GetSelectedBlackboardKey(),
			TargetPawn->GetActorLocation());
		//UE_LOG(LogTemp, Warning, TEXT("Check Sight And Detect_)AlwaysChase true") );
		return;
	}
	
	if (AIController->bIsChasing) // 추적 중인 경우
	{
		// 추적 범위를 벗어났는지 확인
		if (AIController->IsInArrangeOfChaseRange())
		{
			// 추적 범위 내에 있다면 계속 위치를 업데이트
			OwnerComp.GetBlackboardComponent()->SetValueAsVector(GetSelectedBlackboardKey(),
				TargetPawn->GetActorLocation());
		}
		else
		{
			// 추적 범위를 벗어났다면 타겟 정보 초기화
			OwnerComp.GetBlackboardComponent()->ClearValue(GetSelectedBlackboardKey());
		}
	}
	else // 추적 중이 아닌 경우
	{
		// 시야와 탐지 범위 내에 있는지 확인
		if (AIController->LineOfSightTo(TargetPawn) && AIController->IsInArrangeOfDetectRange())
		{
			// 플레이어를 감지하면 블랙보드에 위치와 타겟 정보를 업데이트
			OwnerComp.GetBlackboardComponent()->SetValueAsVector(GetSelectedBlackboardKey(),
				TargetPawn->GetActorLocation());
		}
		else
		{
			// 감지하지 못했다면 타겟 정보 초기화
			OwnerComp.GetBlackboardComponent()->ClearValue(GetSelectedBlackboardKey());
		}
	}
}


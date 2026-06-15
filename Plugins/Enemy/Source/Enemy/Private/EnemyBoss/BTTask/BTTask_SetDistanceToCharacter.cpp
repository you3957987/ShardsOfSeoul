#include "EnemyBoss/BTTask/BTTask_SetDistanceToCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTTask_SetDistanceToCharacter::UBTTask_SetDistanceToCharacter()
{
	NodeName = TEXT("Snapshot Distance");
}

EBTNodeResult::Type UBTTask_SetDistanceToCharacter::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	APawn* ControllingPawn = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr;
	if (nullptr == ControllingPawn)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (nullptr == BlackboardComp) 
	{
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
    
	// 타겟이 없으면 거리를 무한대(매우 큰 값)로 설정하여 잘못된 접근 방지
	if (nullptr == TargetActor)
	{
		BlackboardComp->SetValueAsFloat(DistanceKey.SelectedKeyName, 99999.0f);
		return EBTNodeResult::Succeeded;
	}

	// 거리 계산 및 저장
	float Dist = FVector::Dist(ControllingPawn->GetActorLocation(), TargetActor->GetActorLocation());
	BlackboardComp->SetValueAsFloat(DistanceKey.SelectedKeyName, Dist);

	// 태스크 성공 반환 (즉시 끝남)
	return EBTNodeResult::Succeeded; 
}

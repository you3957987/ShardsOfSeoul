#include "BTTask/BTTask_Patrol.h"
#include "AIController.h"
#include "BaseEnemy.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/TargetPoint.h"

UBTTask_Patrol::UBTTask_Patrol()
{
    NodeName = "PatrolLocationSet";
    // 블랙보드에서 사용할 키 이름을 설정합니다.
    PatrolLocationKey.SelectedKeyName = "PatrolLocation";
    PatrolIndexKey.SelectedKeyName = "PatrolIndex";
}

EBTNodeResult::Type UBTTask_Patrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Super::ExecuteTask(OwnerComp, NodeMemory);

    AAIController* AIController = OwnerComp.GetAIOwner();
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

    if (AIController == nullptr || BlackboardComp == nullptr)
    {
        return EBTNodeResult::Failed;
    }

    ABaseEnemy* Enemy = Cast<ABaseEnemy>(AIController->GetPawn());
    if (Enemy == nullptr)
    {
        return EBTNodeResult::Failed;
    }

    // 순찰 지점이 없는 경우
    if (Enemy->PatrolPoints.Num() == 0)
    {
        // 시작 위치를 순찰 지점으로 설정
        BlackboardComp->SetValueAsVector(PatrolLocationKey.SelectedKeyName, BlackboardComp->GetValueAsVector(TEXT("StartLocation")));
        return EBTNodeResult::Succeeded;
    }

    // 현재 순찰 인덱스를 가져옵니다.
    int32 Index = BlackboardComp->GetValueAsInt(PatrolIndexKey.SelectedKeyName);

    // 다음 순찰 지점을 블랙보드에 설정합니다.
    if (Enemy->PatrolPoints.IsValidIndex(Index) && Enemy->PatrolPoints[Index] != nullptr)
    {
        BlackboardComp->SetValueAsVector(PatrolLocationKey.SelectedKeyName, Enemy->PatrolPoints[Index]->GetActorLocation());

        // 다음 순찰을 위해 인덱스를 업데이트합니다.
        int32 NextIndex = (Index + 1) % Enemy->PatrolPoints.Num();
        BlackboardComp->SetValueAsInt(PatrolIndexKey.SelectedKeyName, NextIndex);

        return EBTNodeResult::Succeeded;
    }

    return EBTNodeResult::Failed;
}



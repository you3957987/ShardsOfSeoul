#include "BTTask/BTTask_RandomEnemyPattern.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_RandomEnemyPattern::UBTTask_RandomEnemyPattern()
{
	NodeName = TEXT("Random Enemy Pattern");
}

EBTNodeResult::Type UBTTask_RandomEnemyPattern::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (PatternNames.Num() == 0)
	{
		return EBTNodeResult::Failed;
	}

	// 배열에서 랜덤 인덱스 선택
	int32 RandomIndex = FMath::RandRange(0, PatternNames.Num() - 1);
	FName SelectedPattern = PatternNames[RandomIndex];

	// 블랙보드 컴포넌트 가져오기
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (BlackboardComp)
	{
		// 선택된 Blackboard Key에 이름 값 설정
		BlackboardComp->SetValueAsName(GetSelectedBlackboardKey(), SelectedPattern);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

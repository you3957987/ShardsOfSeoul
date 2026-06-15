#include "BTTask/BTTask_ClearBlackBoardValue.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_ClearBlackBoardValue::UBTTask_ClearBlackBoardValue()
{
	NodeName = "Clear BlackBoard Value"; // b 트리에 나타날 이름
}

// ExecuteTask 함수는 태스크가 실행될 때 호출됩니다.
EBTNodeResult::Type UBTTask_ClearBlackBoardValue::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	OwnerComp.GetBlackboardComponent()->ClearValue(GetSelectedBlackboardKey()); // 선택된 블랙보드 키의 값을 지웁니다.

	return EBTNodeResult::Succeeded;
}

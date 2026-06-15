#include "BTTask/BTTask_GetRandomFloatValue.h"

#include "BehaviorTree/BlackboardComponent.h"

UBTTask_GetRandomFloatValue::UBTTask_GetRandomFloatValue()
{
	NodeName = "Set Random Float Value";
}

EBTNodeResult::Type UBTTask_GetRandomFloatValue::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp) return EBTNodeResult::Failed;

	// Min ~ Max 사이의 랜덤 값 계산
	float RandomValue = FMath::FRandRange(MinValue, MaxValue);

	// 지정된 블랙보드 키에 값 설정
	BlackboardComp->SetValueAsFloat(SelectedFloatKey.SelectedKeyName, RandomValue);

	return EBTNodeResult::Succeeded;
}
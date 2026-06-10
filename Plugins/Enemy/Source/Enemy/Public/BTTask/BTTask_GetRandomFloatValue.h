#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_GetRandomFloatValue.generated.h"

UCLASS()
class ENEMY_API UBTTask_GetRandomFloatValue : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
protected:
	// 랜덤 값을 저장할 블랙보드 키 (Float 타입이어야 함)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector SelectedFloatKey;

	// 최소값
	UPROPERTY(EditAnywhere, Category = "Random")
	float MinValue = 0.5f;

	// 최대값
	UPROPERTY(EditAnywhere, Category = "Random")
	float MaxValue = 3.0f;
	
public:
	UBTTask_GetRandomFloatValue();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_RandomEnemyPattern.generated.h"


UCLASS()
class ENEMY_API UBTTask_RandomEnemyPattern : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
	
public:
	UBTTask_RandomEnemyPattern();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

public:
	// 에디터에서 설정할 수 있는 패턴 이름 목록
	UPROPERTY(EditAnywhere, Category="자체설정")
	TArray<FName> PatternNames;
};

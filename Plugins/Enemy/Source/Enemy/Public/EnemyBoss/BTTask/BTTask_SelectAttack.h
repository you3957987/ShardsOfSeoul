#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_SelectAttack.generated.h"


// 공격 패턴 이름과 가중치를 담는 구조체
USTRUCT(BlueprintType)
struct FAttackPattern
{
	GENERATED_BODY()

	// 에디터에서 설정할 공격 패턴의 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FName AttackName;

	// 해당 패턴의 가중치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정", meta = (ClampMin = "0.0"))
	FBlackboardKeySelector WeightKey;
};

UCLASS()
class ENEMY_API UBTTask_SelectAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_SelectAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// 공격 패턴과 가중치 목록. 에디터에서 자유롭게 수정할 수 있습니다.
	UPROPERTY(EditAnywhere, Category = "자체설정")
	TArray<FAttackPattern> AttackPatterns;
	
};

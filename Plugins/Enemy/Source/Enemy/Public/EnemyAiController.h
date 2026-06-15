#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAiController.generated.h"

UCLASS()
class ENEMY_API AEnemyAiController : public AAIController
{
	GENERATED_BODY()

	class AGameStateBase* GameState;

	class ABaseEnemy* ControlledEnemy;

	class APawn* Targetpawn;

	UPROPERTY(EditAnywhere, Category="자체설정")
	class UBehaviorTree* BehaviorTree; // B 트리

	class UBlackboardComponent* BlackboardComp; // 블랙보드 컴포넌트
	
protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	
	void PollInit(); // 틱에서 하는 초기화
	bool bFocusToPlayer = false; // 플레이어를 포커스하고 있는지 여부

	bool IsInArrangeOfDetectRange(); // 감지 범위 안에 있는지 여부
	bool IsInArrangeOfChaseRange(); // 추적 범위 안에 있는지 여부

	bool bIsChasing = false; // 추적 중인지 여부
	bool bAlwaysChase = false; // 무조건 플레이어 추적 모드
};

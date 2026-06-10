#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BossAIController.generated.h"


UCLASS()
class ENEMY_API ABossAIController : public AAIController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	//자식에서 추가 구현할 블랙보드 키 설정 함수
	virtual void SetBlackboardKey();

	void PollInit(); // 틱에서 하는 초기화
	
	class AGameStateBase* GameState;
	//class ABaseBossEnemy* ControlledEnemy; 이거는 자식에서 따로따로 해줘야함 !!!!!!!!!!!!!
	class APawn* TargetPawn;
	// B 트리
	UPROPERTY(EditAnywhere, Category="자체설정")
	class UBehaviorTree* BehaviorTree; 
	// 블랙보드 컴포넌트
	class UBlackboardComponent* BlackboardComp; 

public:
	virtual void Tick(float DeltaTime) override;
	
	bool bFocusToPlayer = false; // 타깃 플레이어 설정 후 행동 트리 초기화용
};

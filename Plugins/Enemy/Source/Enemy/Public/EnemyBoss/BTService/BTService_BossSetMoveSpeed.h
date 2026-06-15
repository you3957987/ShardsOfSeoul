#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_BossSetMoveSpeed.generated.h"


UCLASS()
class ENEMY_API UBTService_BossSetMoveSpeed : public UBTService_BlackboardBase
{
	GENERATED_BODY()
	
	float CachedMoveSpeed = 0.0f; // 원래 속도 저장용

protected:
	// 서비스가 활성화될 때 (노드 진입)
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// 서비스가 비활성화될 때 (노드 탈출)
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// 틱 함수 (필요시 사용)
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

public:
	UBTService_BossSetMoveSpeed();
	
	// 변경할 돌진 속도
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float TargetMoveSpeed = 400.0f; 
	
	// [추가] 공격으로 전환할 거리 (이 거리 안에 들어오면 중단)
	UPROPERTY(EditAnywhere, Category = "AttackCheck")
	float AttackRange = 400.0f;
	
	// [추가] 거리를 잴 대상 (플레이어)
	UPROPERTY(EditAnywhere, Category = "AttackCheck")
	FBlackboardKeySelector TargetActorKey;
	
	// [추가] 공격 가능 여부를 설정할 키 (CanAttack)
	UPROPERTY(EditAnywhere, Category = "AttackCheck")
	FBlackboardKeySelector CanAttackKey;
};

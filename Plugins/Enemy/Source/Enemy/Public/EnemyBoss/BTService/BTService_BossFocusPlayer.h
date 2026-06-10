#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_BossFocusPlayer.generated.h"


UCLASS()
class ENEMY_API UBTService_BossFocusPlayer : public UBTService_BlackboardBase
{
	GENERATED_BODY()
	
	// 에디터에서 조절할 회전 속도
	UPROPERTY(EditAnywhere, Category = "AI")
	float RotationSpeed = 5.0f;
	
public:
	UBTService_BossFocusPlayer();
	
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

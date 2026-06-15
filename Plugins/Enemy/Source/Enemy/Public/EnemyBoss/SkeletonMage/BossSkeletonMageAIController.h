#pragma once

#include "CoreMinimal.h"
#include "EnemyBoss/BossAIController.h"
#include "BossSkeletonMageAIController.generated.h"


UCLASS()
class ENEMY_API ABossSkeletonMageAIController : public ABossAIController
{
	GENERATED_BODY()


protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetBlackboardKey() override;

public:
	class ABossSkeletonMage* ControlledEnemy;
};

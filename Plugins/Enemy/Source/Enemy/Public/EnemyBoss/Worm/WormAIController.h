#pragma once

#include "CoreMinimal.h"
#include "EnemyBoss/BossAIController.h"
#include "WormAIController.generated.h"


UCLASS()
class ENEMY_API AWormAIController : public ABossAIController
{
	GENERATED_BODY()
	
protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetBlackboardKey() override;

public:
	class ABossWorm* ControlledEnemy;
};

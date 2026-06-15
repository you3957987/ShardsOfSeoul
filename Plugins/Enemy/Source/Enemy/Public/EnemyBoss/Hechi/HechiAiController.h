#pragma once

#include "CoreMinimal.h"
#include "EnemyBoss/BossAIController.h"
#include "HechiAiController.generated.h"


UCLASS()
class ENEMY_API AHechiAiController : public ABossAIController
{
	GENERATED_BODY()
	
protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetBlackboardKey() override;

public:
	class AHechi* ControlledEnemy;
};

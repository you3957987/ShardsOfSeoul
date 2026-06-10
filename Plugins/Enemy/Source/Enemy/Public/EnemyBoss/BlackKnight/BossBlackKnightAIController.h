#pragma once

#include "CoreMinimal.h"
#include "EnemyBoss/BossAIController.h"
#include "BossBlackKnightAIController.generated.h"


UCLASS()
class ENEMY_API ABossBlackKnightAIController : public ABossAIController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetBlackboardKey() override;

public:
	class ABossBlackKnight* ControlledEnemy;
};

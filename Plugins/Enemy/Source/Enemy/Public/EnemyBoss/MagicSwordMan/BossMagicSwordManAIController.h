#pragma once

#include "CoreMinimal.h"
#include "EnemyBoss/BossAIController.h"
#include "BossMagicSwordManAIController.generated.h"


// 깃 추가 확인용
UCLASS()
class ENEMY_API ABossMagicSwordManAIController : public ABossAIController
{
	GENERATED_BODY()


protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetBlackboardKey() override;

public:
	class ABossMagicSwordMan* ControlledEnemy;
};

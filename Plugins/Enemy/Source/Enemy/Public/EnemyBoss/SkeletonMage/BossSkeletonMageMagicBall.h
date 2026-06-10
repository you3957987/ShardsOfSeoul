#pragma once

#include "CoreMinimal.h"
#include "EnemyProjectile/BaseEnemyProjectile.h"
#include "BossSkeletonMageMagicBall.generated.h"


UCLASS()
class ENEMY_API ABossSkeletonMageMagicBall : public ABaseEnemyProjectile
{
	GENERATED_BODY()

protected:
	virtual void CreateHitEffect() override;
public:
	ABossSkeletonMageMagicBall();
};

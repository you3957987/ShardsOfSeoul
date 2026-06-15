#pragma once

#include "CoreMinimal.h"
#include "EnemyProjectile/BaseEnemyProjectile.h"
#include "HechiMagicBallProjectile.generated.h"


UCLASS()
class ENEMY_API AHechiMagicBallProjectile : public ABaseEnemyProjectile
{
	GENERATED_BODY()

protected:
	virtual void CreateHitEffect() override;
public:
	AHechiMagicBallProjectile();
};

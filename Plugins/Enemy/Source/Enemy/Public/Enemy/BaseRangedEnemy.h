#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "BaseRangedEnemy.generated.h"


UCLASS()
class ENEMY_API ABaseRangedEnemy : public ABaseEnemy
{
	GENERATED_BODY()

protected:
	
	// 원거리 공격 타입일 때 발사체 클래스
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	TSubclassOf<class ABaseEnemyProjectile> ProjectileClass;

	// 원거리 공격 지점 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* RangedAttackPoint;
	
public:
	ABaseRangedEnemy();

	UFUNCTION(BlueprintCallable)
	void ShootProjectile();
};

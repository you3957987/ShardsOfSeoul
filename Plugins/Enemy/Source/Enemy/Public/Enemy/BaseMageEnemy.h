#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "BaseMageEnemy.generated.h"


UCLASS()
class ENEMY_API ABaseMageEnemy : public ABaseEnemy
{
	GENERATED_BODY()
	
	
	
	// 안개 공격 함수 - 애님 노티파이에서 호출
	UFUNCTION(BlueprintCallable)
	void CastFogAttack();
	// 지속 대미지 발사체 클래스
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	TSubclassOf<class ADamageZoneProjectile> DamageZoneProjectileClass;
	
	// 폭발 발사체 발사 함수
	UFUNCTION(BlueprintCallable)
	void CastExplosionAttack();
	// 폭발 발사체 클래스
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	TSubclassOf<class ABaseDelayedBurstProjectile> ExplosionProjectileClass;
	
	
public:
	ABaseMageEnemy();
	
};

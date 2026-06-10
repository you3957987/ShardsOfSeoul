#pragma once

#include "CoreMinimal.h"
#include "EnemyProjectile/BaseEnemyProjectile.h"
#include "BossWormProjectile.generated.h"

UCLASS()
class ENEMY_API ABossWormProjectile : public ABaseEnemyProjectile
{
	GENERATED_BODY()
	
protected:
	virtual void CreateHitEffect() override;
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	void CreateDamageZoneOnGround();
	
	// 장판 발사체 클래스
	UPROPERTY(EditAnywhere, Category="자체설정")
	TSubclassOf<class ADamageZoneProjectile> DamageZoneProjectileClass;
	
public:
	ABossWormProjectile();
};

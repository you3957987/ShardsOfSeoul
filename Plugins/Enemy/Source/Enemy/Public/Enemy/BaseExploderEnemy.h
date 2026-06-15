#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "BaseExploderEnemy.generated.h"


UCLASS()
class ENEMY_API ABaseExploderEnemy : public ABaseEnemy
{
	GENERATED_BODY()

private:
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* ExplosionRangeSphere;

protected:
	UPROPERTY(EditAnywhere, Category="자체설정")
	float ExplosionRange = 200.f;

	UPROPERTY(EditAnywhere, Category="자체설정")
	float ExplosionDamage = 50.f;

	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	class UNiagaraSystem* ExplosionEffect;

public:
	ABaseExploderEnemy();
	
	UFUNCTION(BlueprintCallable)
	void Explode();

#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseDelayedBurstProjectile.generated.h"

UCLASS()
class ENEMY_API ABaseDelayedBurstProjectile : public AActor
{
	GENERATED_BODY()
	
	// 폭발 대기 시간
	UPROPERTY(EditAnywhere, Category="자체설정")
	float DelayBeforeBurst = 2.f;
	
	// 폭발시 대미지
	UPROPERTY(EditAnywhere, Category="자체설정")
	float ExplosionDamage = 20.f;
	
	// 대미지 줄 스피어 콜리전
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* DamageSphere;

	// 폭발 전 유지되는 나이아가라 이펙트 (퓨즈/차징 등)
	UPROPERTY(EditAnywhere, Category = "자체설정")
	class UNiagaraSystem* PreExplosionEffect;

	// 폭발 시 재생되는 나이아가라 이펙트
	UPROPERTY(EditAnywhere, Category = "자체설정")
	class UNiagaraSystem* ExplosionEffect;

	// 폭발시 일어날 2D 사운드
	UPROPERTY(EditAnywhere, Category = "자체설정")
	class USoundBase* ExplosionSound;

	// 타이머에 의해 호출되어 실제 폭발을 수행하는 함수
	UFUNCTION()
	void Explode();
	
protected:
	virtual void BeginPlay() override;
	
public:	
	ABaseDelayedBurstProjectile();
	virtual void Tick(float DeltaTime) override;
	
	// 생성된 전조 이펙트 관리용 컴포넌트
	UPROPERTY()
	class UNiagaraComponent* PreExplosionComp;
	
	FTimerHandle ExplosionTimerHandle;

};

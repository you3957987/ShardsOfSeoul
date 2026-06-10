#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseStreamProjectile.generated.h"

UCLASS()
class ENEMY_API ABaseStreamProjectile : public AActor
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:	
	ABaseStreamProjectile();
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category="자체설정")
	bool bOnlyNiagaraEffect = false; // 나이아가라 이펙트만 사용할지 여부
	// 발사체 속도
	UPROPERTY(EditAnywhere, Category="자체설정")
	float ProjectileSpeed = 3000.f;
	// 데미지
	UPROPERTY(EditAnywhere, Category="자체설정")
	float Damage = 2.f; 
	// 방사 지속 시간 == 사정거리
	UPROPERTY(EditAnywhere, Category="자체설정")
	float Duration = 0.2f;
	// 유지 시간 위한 타이머 핸들
	FTimerHandle DurationTimerHandle;
	void DeactivateZone();
	
	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	// 충돌 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USphereComponent* CollisionComp;
	
	// 나이아가라 이펙트 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UNiagaraComponent* NiagaraEffectComp;
	
	// 투사체 이동 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UProjectileMovementComponent* ProjectileMovement;
	
#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

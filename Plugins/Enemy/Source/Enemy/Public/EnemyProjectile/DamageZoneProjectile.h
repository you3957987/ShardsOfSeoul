#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageZoneProjectile.generated.h"

UCLASS()
class ENEMY_API ADamageZoneProjectile : public AActor
{
	GENERATED_BODY()


protected:
	virtual void BeginPlay() override;

public:	
	ADamageZoneProjectile();
	virtual void Tick(float DeltaTime) override;
	
	// 루트가 될 씬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USceneComponent* RootSceneComponent;
	
	// 오버랩 감지용 메쉬 (원통 메쉬를 여기에 할당)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UStaticMeshComponent* OverlapMesh;
	
	// 나이아가라 컴포넌트
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UNiagaraComponent* NiagaraEffectComp;
	
	// 장판 유지 시간
	UPROPERTY(EditAnywhere, Category="자체설정")
	float ZoneDuration = 5.f;
	
	// 대미지
	UPROPERTY(EditAnywhere, Category="자체설정")
	float DamageAmount = 10.f;
	
	// 대미지 적용 간격	
	UPROPERTY(EditAnywhere, Category="자체설정")
	float CheckInterval = 0.3f;
	
	FTimerHandle CheckTimerHandle;
	void ApplyDamageToOverlappingActors();

	FTimerHandle DurationTimerHandle;
	void DeactivateZone();
};

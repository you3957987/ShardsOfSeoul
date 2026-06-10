#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GroundAttackProjectile.generated.h"

UCLASS()
class ENEMY_API AGroundAttackProjectile : public AActor
{
	GENERATED_BODY()

	FVector InitialLocation;
	FVector PeakLocation;
	float RiseElapsedTime = 0.f;
	float StayElapsedTime = 0.f;
	float LowerElapsedTime = 0.f;

	bool bIsRising = false;
	bool bIsStaying = false;
	bool bIsLowering = false;

	void UpperMesh(float DeltaTime);
	void LowerMesh(float DeltaTime);
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category="자체설정")
	float Damage = 20.f; // 데미지

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* RootComp;
	
	// 메시 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* MeshComp;
	
public:	
	AGroundAttackProjectile();
	virtual void Tick(float DeltaTime) override;
	
	void HandleDamage();
	
	// 생성 후 상승할 높이
	UPROPERTY(EditAnywhere, Category="자체설정")
	float RiseHeight = 20.f;
	// 상승하는 데 걸리는 시간
	UPROPERTY(EditAnywhere, Category="자체설정")
	float RiseDuration = 0.2f;
	// 하강에 걸리는 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float LowerDuration = 1.f; 
	// 장판 유지 시간
	UPROPERTY(EditAnywhere, Category="자체설정")
	float DurationTime = 5.f;
};

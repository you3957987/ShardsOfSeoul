#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PingActor.generated.h"

UCLASS()
class PET_API APingActor : public AActor
{
	GENERATED_BODY()

	FVector TargetLocation;
	FVector StartLocation;     // 시작 위치 저장 (방향 계산용)
	FVector CurrentLinearPos;  // 흔들림 없는 '가상의 직선 위치'
	
	bool bIsMoving = false;
	float MoveSpeed = 600.0f; // 단위: cm/s
	float RunningTime;         // 사인 함수용 시간 누적
	float TotalDistance;   
	
protected:
	virtual void BeginPlay() override;

	// 루트 컴포넌트 (충돌체 겸 위치 기준)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "자체설정")
	class USphereComponent* SphereComp;

	// 시각적 이펙트 (나이아가라)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "자체설정")
	class UNiagaraComponent* EffectComp;

	// [설정] 흔들림 폭 (얼마나 넓게 왔다갔다 할지)
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float SwayAmplitude = 50.0f;

	// [설정] 흔들림 속도 (얼마나 빨리 왔다갔다 할지)
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float SwayFrequency = 10.0f;
	
public:	
	APingActor();
	virtual void Tick(float DeltaTime) override;

	void StartPingMovement(const FVector& TargetPos);

};

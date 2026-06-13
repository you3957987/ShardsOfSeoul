#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackholeProjectile.generated.h"

UCLASS()
class ENEMY_API ABlackholeProjectile : public AActor
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// 플레이어 감지용 스피어 콜리전
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USceneComponent* SceneComponent;
	
	// 캐스케이드 파티클 시스템 나올 컴포넌트	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UParticleSystemComponent* ParticleSystemComp;
	
	// 끌어당기는 힘의 세기 (인력이므로 음수 값을 사용해야 당겨집니다!)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float PullStrength = -1000000.f;
	
	// 블랙홀 유지 시간 (초 단위, 기본값 5초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float LifeTime = 5.0f;

	// 유지 시간 타이머 관리를 위한 핸들
	FTimerHandle BlackholeEndTimerHandle;
	
	FTimerHandle DestroyTimerHandle;
	
	// 유지 시간이 끝났을 때 실행할 특정 함수
	void BlackholeEnd();
	
	bool BlackHoleFlag = true;
	
public:	
	ABlackholeProjectile();
	virtual void Tick(float DeltaTime) override;


#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "BaseMimicEnemy.generated.h"


UCLASS()
class ENEMY_API ABaseMimicEnemy : public ABaseEnemy
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	// 근접 공격 데미지
	UPROPERTY(EditAnywhere, Category="자체설정")
	float MeleeAttackDamage = 20.f; //

	// 근접 공격 지점 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* MeleeAttackPoint;
	// 근접 공격 범위
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* AttackRangePointSphere;

	void CheckMeleeAttackHit(float DeltaTime); // 공격 히트 체크 함수


	// 위장용 스태틱 메시 (상자, 보물상자, 바위 등)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UStaticMeshComponent* DisguiseMesh;
	// 플레이어 감지용 트리거 스피어 (실제 오버랩 이벤트 발생용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USphereComponent* MimicTriggerSphere;
	// 플레이어가 인식 범위(DetectRangeSphere)에 들어왔을 때 실행할 델리게이트 함수
	UFUNCTION()
	void OnDetectOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	// 위장 해제 함수 (스태틱 메시 숨김 -> 캐릭터 메시 보임 -> AI 활성화)
	void WakeUp();
	bool bIsHiding = true; // 현재 위장 상태를 나타내는 변수

	// 깨어날 때 생성할 이펙트 (캐스케이드)
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	class UParticleSystem* WakeUpEffect;
	
public:
	ABaseMimicEnemy();
	virtual void Tick(float DeltaTime) override;

	// 이미 공격에 히트된 액터들을 저장하는 배열
	UPROPERTY()
	TArray<TObjectPtr<AActor>> HittedActors;
	// 근접 공격 중인지 여부를 나타내는 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "자체설정")
	bool bIsMeleeAttacking = false; 
	// 근접 공격 범위 활성화 함수 - 애님 노티파이에서 호출
	UFUNCTION(BlueprintCallable)
	void AttackCheck_Start() { HittedActors.Empty();bIsMeleeAttacking = true;} ;
	// 근접 공격 범위 비활성화 함수 - 애님 노티파이에서 호출
	UFUNCTION(BlueprintCallable)
	void AttackCheck_End() { HittedActors.Empty();bIsMeleeAttacking = false; };


#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

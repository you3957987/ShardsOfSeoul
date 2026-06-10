#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "BaseSlimeEnemy.generated.h"


UCLASS()
class ENEMY_API ABaseSlimeEnemy : public ABaseEnemy
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// 분열 시 생성할 슬라임 수. 0이면 분열 안함
	UPROPERTY(EditAnywhere, Category="자체설정")
	int32 SplitCount = 2; 

	// 분열 시 생성할 슬라임 클래스
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	TSubclassOf<ABaseSlimeEnemy> SplitSlimeClass;

	// 분열 시 생성할 슬라임 간격
	UPROPERTY(EditAnywhere, Category="자체설정")
	float SplitSpawnRadius = 100.f;

	// 분열시 슬라임 체력 배율
	UPROPERTY(EditAnywhere, Category="자체설정")
	float SplitSlimeHealthPercent = 0.5f; // 원래 체력의 50%

	// 분열시 슬라임 크기 배율
	UPROPERTY(EditAnywhere, Category="자체설정")
	float SplitSlimeScalePercent = 0.7f; // 원래 크기의 70%

	//분열시 슬라임 근접공격 데미지 배율
	UPROPERTY(EditAnywhere, Category="자체설정")
	float SplitSlimeMeleeDamage = 0.5f; // 원래 데미지의 50%
	
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


public:
	ABaseSlimeEnemy();
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

	// 죽고 나서 분열처리할 함수 - 애님 노티파이에서 호출
	virtual void SpawnDeadEffectAndDestroy() override;

	
#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

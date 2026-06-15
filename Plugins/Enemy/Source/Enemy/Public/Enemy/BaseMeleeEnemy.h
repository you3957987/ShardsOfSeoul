#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "BaseMeleeEnemy.generated.h"


UCLASS()
class ENEMY_API ABaseMeleeEnemy : public ABaseEnemy
{
	GENERATED_BODY()

protected:
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
	ABaseMeleeEnemy();
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

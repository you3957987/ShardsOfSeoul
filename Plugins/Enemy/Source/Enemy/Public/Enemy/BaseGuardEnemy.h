#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "BaseGuardEnemy.generated.h"


UCLASS()
class ENEMY_API ABaseGuardEnemy : public ABaseEnemy
{
	GENERATED_BODY()

public:
	ABaseGuardEnemy();
	virtual void BeginPlay() override;
	virtual UAnimMontage* Attack() override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;
	
	UPROPERTY(BlueprintReadWrite)
	bool bIsGuarding = false; // 방어 중인지 여부를 나타내는 변수
	
	class UCapsuleComponent* WeaponCollision = nullptr;
	
	float AttackDamage = 0.f;
	
	// 공격 대미지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "자체설정")
	float NormalAttackDamage = 20.f;
	// 가드중 받은 대미지 누적량
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "자체설정")
	float DamageWhileGuarding = 0.f;
	// 가드 공격으로 간주할 대미지 누적량 임계값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "자체설정")
	float MaxDamageToReaction = 30.f;
	// 가드 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "자체설정")
	float GuardDuration = 5.f;
	
	// 가드시 플레이할 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* GuardMontage;
	// 가드 리액션 공격 몽타주 
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* GuardReactionAttackMontage;
	// 가드 리액션 공격 대미지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "자체설정")
	float GuardReactionDamage = 40.f;
	// 가드 공격 함수
	void GuardReactionAttack();
	
	//공격 범위에 플레이어가 들어왔을 때 호출되는 함수
	UFUNCTION()
	void OnBeginOverlapWeaponCollisionSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION(BlueprintCallable)
	void AttackStart_WeaponCollision();
	UFUNCTION(BlueprintCallable)
	void AttackEnd_WeaponCollision();
};

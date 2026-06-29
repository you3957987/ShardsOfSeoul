#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SprintComp.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SHARDSOFSEOUL_API USprintComp : public UActorComponent
{
	GENERATED_BODY()
public:
	USprintComp();
	
protected:
	virtual void BeginPlay() override;
	
public:
	// 대시 시작 및 종료 함수
	UFUNCTION(BlueprintCallable, Category = "Movement|Sprint")
	void StartSprint();

	UFUNCTION(BlueprintCallable, Category = "Movement|Sprint")
	void StopSprint();
	
	UFUNCTION(BlueprintCallable, Category = "Movement|Sprint")
	FORCEINLINE bool IsSprinting() const { return bIsSprinting; }
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Sprint")
	float WalkSpeed = 500.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Sprint")
	float SprintSpeed = 700.f;
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Sprint", meta = (AllowPrivateAccess = "true"))
	// 현재 대시 중인지 여부
	bool bIsSprinting = false;

	// 오너 캐릭터의 무브먼트 컴포넌트 캐싱
	UPROPERTY()
	class UCharacterMovementComponent* MovementComponent;
};

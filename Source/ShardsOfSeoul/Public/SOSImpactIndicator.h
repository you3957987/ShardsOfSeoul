#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SOSImpactIndicator.generated.h"

UCLASS()
class SHARDSOFSEOUL_API ASOSImpactIndicator : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASOSImpactIndicator();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 블루프린트 타임라인 업데이트에서 호출될 함수
	UFUNCTION(BlueprintCallable, Category = "Combat|Impact")
	void UpdateMPCSpread(float RadiusValue);

protected:
	// 에디터에서 지정하거나 동적으로 할당할 MPC 에셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Impact")
	class UMaterialParameterCollection* MPC_Coloring;

	// 업데이트할 스칼라 파라미터 이름 (기본값 "Radius")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Impact")
	FName RadiusParameterName;
};

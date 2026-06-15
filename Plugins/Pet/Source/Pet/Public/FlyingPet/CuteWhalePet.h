#pragma once

#include "CoreMinimal.h"
#include "BaseFlyingPet.h"
#include "CuteWhalePet.generated.h"

UCLASS()
class PET_API ACuteWhalePet : public ABaseFlyingPet
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// 기본 머티리얼들 
	UPROPERTY(EditAnywhere, Category = "자체설정")
	UMaterialInterface* DefaultColorMaterial;
	// 기본 머티리얼들 
	UPROPERTY(EditAnywhere, Category = "자체설정")
	UMaterialInterface* DefaultFaceMaterial;
	
	UPROPERTY(EditAnywhere, Category = "자체설정")
	TArray<UMaterialInterface*> ColorMaterials;

	UPROPERTY(EditAnywhere, Category = "자체설정")
	TArray<UMaterialInterface*> FaceMaterials;

public:
	ACuteWhalePet();
	
	// 색상과 표정 인덱스를 받아 머티리얼을 변경하는 함수 추가
	UFUNCTION(BlueprintCallable)
	void SetPetAppearance(int32 ColorIndex, int32 FaceIndex);
};

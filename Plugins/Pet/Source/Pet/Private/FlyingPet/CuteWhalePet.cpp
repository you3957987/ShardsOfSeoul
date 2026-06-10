#include "FlyingPet/CuteWhalePet.h"

ACuteWhalePet::ACuteWhalePet()
{
	
}

void ACuteWhalePet::BeginPlay()
{
	Super::BeginPlay();

	
}

void ACuteWhalePet::SetPetAppearance(int32 ColorIndex, int32 FaceIndex)
{
	USkeletalMeshComponent* TargetMesh = MeshComp;

	if (TargetMesh)
	{
		// 0번 슬롯: 몸통 색상 변경
		if (ColorMaterials.IsValidIndex(ColorIndex))
		{
			TargetMesh->SetMaterial(0, ColorMaterials[ColorIndex]);
		}

		// 1번 슬롯: 얼굴 표정 변경
		if (FaceMaterials.IsValidIndex(FaceIndex))
		{
			TargetMesh->SetMaterial(1, FaceMaterials[FaceIndex]);
		}
	}
}


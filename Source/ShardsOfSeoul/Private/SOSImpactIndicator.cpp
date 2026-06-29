#include "SOSImpactIndicator.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

// Sets default values
ASOSImpactIndicator::ASOSImpactIndicator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MPC_Coloring = nullptr;
	RadiusParameterName = FName("Radius");
}

// Called when the game starts or when spawned
void ASOSImpactIndicator::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ASOSImpactIndicator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASOSImpactIndicator::UpdateMPCSpread(float RadiusValue)
{
	if (MPC_Coloring && GetWorld())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), MPC_Coloring, RadiusParameterName, RadiusValue);
	}
}

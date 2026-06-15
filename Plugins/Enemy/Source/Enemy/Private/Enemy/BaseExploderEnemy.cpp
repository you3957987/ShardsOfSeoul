#include "Enemy/BaseExploderEnemy.h"

#include "EnemyLogManager.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseExploderEnemy::ABaseExploderEnemy()
{
	ExplosionRangeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ExplosionRangeSphere"));
	ExplosionRangeSphere->SetupAttachment(RootComponent); // 루트 컴포넌트
	ExplosionRangeSphere->ShapeColor = FColor::Yellow;
	ExplosionRangeSphere->SetVisibility(false);
	ExplosionRangeSphere->SetHiddenInGame(false);


	EnemyType = EEnemyType::EET_Exploder;
}

void ABaseExploderEnemy::Explode()
{
	// 한 프레임 오버랩 검사: ExplosionRangeSphere와 겹치는 액터들을 가져와 태그로 필터링
	if (ExplosionRangeSphere)
	{
		TArray<AActor*> OverlappingActors;
		ExplosionRangeSphere->GetOverlappingActors(OverlappingActors);

		for (AActor* Actor : OverlappingActors)
		{
			if (Actor && Actor->ActorHasTag(FName("Player")))
			{
				UE_LOG(LogTemp, Warning, TEXT("Explode overlap with Player: %s"), *Actor->GetName());

				// 로그 기록 로직
				if (GetMesh()) 
				{
					// 스켈레탈 메쉬 에셋 이름 가져오기
					FString MeshName = GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
					
					UEnemyLogManager::EnemyLog(EEnemyLogType::Exploder, 
						FString::Printf(TEXT("적 [%s]가 [%.f] 대미지"), 
							*MeshName, 
							ExplosionDamage)
					);
				}
				
				EnemyLogData.TotalDamageDealt += ExplosionDamage; // 로그 데이터에 입힌 대미지 누적
				
				//액터에게 데미지 적용
				UGameplayStatics::ApplyDamage(
					Actor,
					ExplosionDamage,
					GetController(),
					this,
					UDamageType::StaticClass()
				);
			}
		}
	}
	
	if (ExplosionEffect && ExplosionRangeSphere && GetWorld() )
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ExplosionEffect,
			ExplosionRangeSphere->GetComponentLocation(),
			ExplosionRangeSphere->GetComponentRotation()
		);
	}
	Die(); // 폭발 후 죽음 처리
}

#if WITH_EDITOR
void ABaseExploderEnemy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr)
									? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseExploderEnemy, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( ExplosionRangeSphere ) ExplosionRangeSphere->SetVisibility(true);
		}
		else
		{
			if ( ExplosionRangeSphere ) ExplosionRangeSphere->SetVisibility(false);
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseExploderEnemy, ExplosionRange))
	{
		if (ExplosionRangeSphere)
		{
			ExplosionRangeSphere->SetSphereRadius(ExplosionRange);
		}
	}
}
#endif
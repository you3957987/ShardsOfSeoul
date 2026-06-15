#include "Enemy/BaseTransparEnemy.h"

#include "EnemyLogManager.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseTransparEnemy::ABaseTransparEnemy()
{
	MeleeAttackPoint = CreateDefaultSubobject<USceneComponent>(TEXT("AttackPoint"));
	MeleeAttackPoint->SetupAttachment(RootComponent); // 루트 컴포넌트
	
	AttackRangePointSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AttackRangePointSphere"));
	AttackRangePointSphere->SetupAttachment(MeleeAttackPoint); // AttackPoint에 부착
	AttackRangePointSphere->ShapeColor = FColor::Purple;
	AttackRangePointSphere->SetVisibility(false);
	AttackRangePointSphere->SetHiddenInGame(false);

	EnemyType = EEnemyType::EET_Transpar;
	
	Tags.Add("Trans");
}

void ABaseTransparEnemy::BeginPlay()
{
	Super::BeginPlay();

	SetCharacterTransparency(true); // 시작할 때 투명 상태로 설정
	TestTransparencyLogic(); // 테스트용 투명화 로직 함수
}

void ABaseTransparEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckMeleeAttackHit(DeltaTime);
}

UAnimMontage* ABaseTransparEnemy::Attack()
{
	// 공격 시 투명 상태라면 투명화 해제
	if ( bIsTransparent == true ) SetCharacterTransparency(false);
	
	FTimerHandle DelayTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(DelayTimerHandle, [this]()
	{
		return Super::Attack();
	}, 0.3f, false); // 특정 지연 후에 실행
	
	return nullptr;
}

float ABaseTransparEnemy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	// 부모 클래스의 TakeDamage를 호출하여 기본 데미지 처리를 수행합니다.
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 실제로 데미지를 입었다면 투명화를 해제합니다.
	if (ActualDamage > 0.f && bIsTransparent == true) 
	{
		SetCharacterTransparency(false);
	}
	
	return ActualDamage;
}

void ABaseTransparEnemy::CheckMeleeAttackHit(float DeltaTime)
{
	if (bIsMeleeAttacking == true && EnemyType == EEnemyType::EET_Transpar) // 투명 근접 공격 타입이고 공격 중일 때
	{
		TArray<AActor*> OverlappingActors;
		// AttackRangePointSphere와 겹치는 모든 액터를 가져옵니다.
		AttackRangePointSphere->GetOverlappingActors(OverlappingActors);
		
		for (AActor* OverlappingActor : OverlappingActors)
		{
			// 액터가 유효하고 "Player" 태그를 가지고 있으며, 아직 공격한 목록에 없는지 확인합니다.
			if (OverlappingActor && OverlappingActor->ActorHasTag(FName("Player")) && !HittedActors.Contains(OverlappingActor))
			{
				UE_LOG(LogTemp, Warning, TEXT("Attack Hit Detected on: %s"), *OverlappingActor->GetName());
				
				// 로그 기록 로직
				if (GetMesh()) 
				{
					// 스켈레탈 메쉬 에셋 이름 가져오기
					FString MeshName = GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
					
					UEnemyLogManager::EnemyLog(EEnemyLogType::Transpar, 
						FString::Printf(TEXT("적 [%s]가 [%.f] 대미지"), 
							*MeshName, 
							MeleeAttackDamage));
				}
				
				EnemyLogData.TotalDamageDealt += MeleeAttackDamage; // 로그 데이터에 입힌 대미지 누적
				
				// 플레이어에게 대미지를 적용합니다.
				UGameplayStatics::ApplyDamage(
					OverlappingActor,
					MeleeAttackDamage, // 헤더 파일에 선언된 대미지 변수
					GetController(),
					this,
					UDamageType::StaticClass()
				);
				
				// 공격한 목록에 추가하여 중복 피해를 방지합니다.
				HittedActors.Add(OverlappingActor);

				bIsMeleeAttacking = false; // 공격 상태를 종료합니다.
			}
		}
	}
}

void ABaseTransparEnemy::SetCharacterTransparency(bool bMakeTransparent)
{
	bIsTransparent = bMakeTransparent;

	if ( TransparChangeEffect && GetWorld() )
	{
		// 현재 위치 + (앞방향 * 앞뒤 오프셋) + (윗방향 * 위아래 오프셋)
		const FVector SpawnLocation = GetActorLocation()
			+ (GetActorForwardVector() * TransEffectForwardOffset)
			+ (GetActorUpVector() * TransEffectUpOffset);

		// 나이아가라 이펙트를 계산된 위치에 생성합니다.
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),
			TransparChangeEffect, SpawnLocation, GetActorRotation());

	
		FTimerHandle DelayTimerHandle;
		// 이펙트 생성 이후 일정 시간 이후에 투명도 변경
		GetWorld()->GetTimerManager().SetTimer(DelayTimerHandle, [this, bMakeTransparent]()
		{
			// HealthBarWidget의 가시성을 설정합니다.
			if (HealthBarWidget) HealthBarWidget->SetVisibility(!bMakeTransparent);

			// 모든 메시 컴포넌트를 순회합니다.
			TArray<UMeshComponent*> MeshComponents;
			GetComponents<UMeshComponent>(MeshComponents);

			for (UMeshComponent* MeshComp : MeshComponents)
			{
				if (!MeshComp) continue;

				// TransparencyData 배열에서 현재 메시 컴포넌트와 이름이 일치하는 데이터를 찾습니다.
				for (const FTransparencyMaterialData& Data : TransparencyData)
				{
					if (Data.MeshComponentName == MeshComp->GetFName())
					{
						// 해당 메시 컴포넌트의 머티리얼 슬롯별 데이터를 순회합니다.
						for (const FMaterialSlotTransparencyData& SlotData : Data.MaterialSlots)
						{
							// 설정할 머티리얼을 결정합니다.
							UMaterialInterface* MaterialToSet = bMakeTransparent
								? SlotData.TransparentMaterial
								: SlotData.OpaqueMaterial;

							// 머티리얼을 교체합니다.
							if (MaterialToSet)
							{
								MeshComp->SetMaterial(SlotData.MaterialSlotIndex, MaterialToSet);
							}
						}
						break;
					}
				}
			}
		}, 0.1f, false); // 특정 지연 후에 실행
	}
}

void ABaseTransparEnemy::TestTransparencyLogic()
{
	if (bCheckTransparencyLogic)
	{
		FTimerHandle TransparencyTestTimerHandle;
		// 5초 후에 투명 상태를 반전시키는 람다 함수를 호출합니다.
		GetWorld()->GetTimerManager().SetTimer(TransparencyTestTimerHandle, [this]()
		{
			SetCharacterTransparency(!bIsTransparent);
		}, 5.0f, true); // true로 설정하여 반복 타이머로 만듭니다.
	}
}

#if WITH_EDITOR
void ABaseTransparEnemy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseTransparEnemy, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( AttackRangePointSphere ) AttackRangePointSphere->SetVisibility(true);
		}
		else
		{
			if ( AttackRangePointSphere ) AttackRangePointSphere->SetVisibility(false);
		}
	}
}
#endif
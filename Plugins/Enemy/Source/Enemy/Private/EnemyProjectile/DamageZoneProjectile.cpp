#include "EnemyProjectile/DamageZoneProjectile.h"

#include "EnemyLogManager.h"
#include "NiagaraComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

ADamageZoneProjectile::ADamageZoneProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	RootComponent = RootSceneComponent;
	
	OverlapMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OverlapMesh"));
	OverlapMesh->SetupAttachment(RootComponent);
	
	NiagaraEffectComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraEffect"));
	NiagaraEffectComp->SetupAttachment(RootComponent);
	
	// 콜리전 설정 (오버랩 감지용)
	OverlapMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OverlapMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	OverlapMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	OverlapMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	// 눈에 보이지 않게 설정 
	OverlapMesh->SetVisibility(false);
	
}

void ADamageZoneProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	//SetLifeSpan(ZoneDuration); 이거 대신 타이머
	// ZoneDuration 후에 DeactivateZone 함수 실행
	GetWorld()->GetTimerManager().SetTimer(DurationTimerHandle, this, 
		&ADamageZoneProjectile::DeactivateZone, ZoneDuration, false);
	
	GetWorld()->GetTimerManager().SetTimer(CheckTimerHandle, this, 
		&ADamageZoneProjectile::ApplyDamageToOverlappingActors, CheckInterval, true);
}

void ADamageZoneProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// 타이머에 의해 주기적으로 호출될 함수
void ADamageZoneProjectile::ApplyDamageToOverlappingActors()
{
	TArray<AActor*> OverlappingActors;
	OverlapMesh->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		// 자기 자신(예: 몬스터)은 제외하려면 태그나 클래스 검사 추가
		if (Actor && Actor != this && Actor != GetOwner() && Actor->ActorHasTag(FName("Player")) ) // 플레이어만 데미지 적용
		{
			// 로그로 대미지 양 확인 (필요시 주석 해제)
			UE_LOG(LogTemp, Warning, TEXT("Damage Zone applied %f damage "), DamageAmount);
			
			// 1. 발사체의 소유자(Owner)를 가져와서 캐릭터인지 확인
			if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
			{
				if (OwnerCharacter->GetMesh())
				{
					// 2. 주인 캐릭터의 스켈레탈 메쉬 에셋 이름 가져오기
					FString OwnerMeshName = OwnerCharacter->GetMesh()->GetSkeletalMeshAsset() ? 
						OwnerCharacter->GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");

					// 3. 메시 이름에 "Worm"이 포함되어 있는지 확인
					if (OwnerMeshName.Contains(TEXT("WormMonster_SK")))
					{
						// 웜 보스 전용 로그
						UEnemyLogManager::EnemyLog(EEnemyLogType::Worm, 
							FString::Printf(TEXT("[웜] 화염 장판 적중 | 대미지[%.f]"), DamageAmount));
					}
					else
					{
						// 그 외(메이지 등) 일반 로그
						UEnemyLogManager::EnemyLog(EEnemyLogType::Mage, 
							FString::Printf(TEXT("적 [%s]가 대미지 유지 발사체가 [%.f] 대미지 줌"), *OwnerMeshName, DamageAmount));
					}
				}
			}
			
			// 여기서 데미지 적용 또는 효과 부여
			UGameplayStatics::ApplyDamage(Actor, DamageAmount, GetInstigatorController(), 
				this, UDamageType::StaticClass());
		}
	}
}

void ADamageZoneProjectile::DeactivateZone()
{
	//  더 이상 데미지를 주지 않도록 타이머/콜리전 끄기
	GetWorld()->GetTimerManager().ClearTimer(CheckTimerHandle);
	OverlapMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//  나이아가라 이펙트의 신규 생성을 중지 (이미 생성된 파티클은 자연스럽게 남음)
	if (NiagaraEffectComp)
	{
		NiagaraEffectComp->Deactivate();
	}

	// 기존 파티클이 사라질 충분한 시간을 주고 삭제
	SetLifeSpan(2.0f); 
}
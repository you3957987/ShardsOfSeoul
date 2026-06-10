#include "Trigger/ConversationTrigger.h"

#include "Components/BoxComponent.h"
#include "Interface/PetConversationInterface.h"

AConversationTrigger::AConversationTrigger()
{
	// 틱 비활성화
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetHiddenInGame(true); 
}

void AConversationTrigger::BeginPlay()
{
	Super::BeginPlay();

	if ( TriggerBox )
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AConversationTrigger::OnTriggerBoxBeginOverlap);
	}
}

void AConversationTrigger::OnTriggerBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 1. Actor와 OtherComp가 유효한지 확인
	if (OtherActor && OtherComp && OtherActor->ActorHasTag(TargetTag))
	{
		// 겹친 컴포넌트(OtherComp)가 액터의 루트 컴포넌트인지 확인, 인터페이스 구현 확인
		if (OtherComp == OtherActor->GetRootComponent() && OtherActor->Implements<UPetConversationInterface>())
		{

			if ( ConversationType == EConversationType::ECT_BIG )
			{
				// 3. 인터페이스 함수 실행 == 이게 펫이던 플레이어던 하나 얻어걸리는 식으로 펫, 플레이어용 함수 둘 다 실행

				// 펫 기준
				IPetConversationInterface::Execute_TriggerPetBigConversation(OtherActor, DialogueID);
				// 펫 주인 기준
				IPetConversationInterface::Execute_MasterToPetBigConversation(OtherActor, DialogueID);
				Destroy(); 
			}
			else if ( ConversationType == EConversationType::ECT_SMALL )
			{
				// 펫 기준
				IPetConversationInterface::Execute_TriggerPetSmallConversation(OtherActor, DialogueID);
				
				// 펫 주인 기준
				IPetConversationInterface::Execute_MasterToPetSmallConversation(OtherActor, DialogueID);
				Destroy();
			}
		}
	}
}

#if WITH_EDITOR
void AConversationTrigger::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AConversationTrigger, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( TriggerBox ) TriggerBox->SetHiddenInGame(false);
		}
		else
		{
			if ( TriggerBox ) TriggerBox->SetHiddenInGame(true); 
		}
	}
}
#endif



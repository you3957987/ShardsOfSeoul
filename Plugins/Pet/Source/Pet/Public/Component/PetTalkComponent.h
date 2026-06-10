#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PetTalkComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConversationEnded);

// 대화 데이터 구조체 (FTableRowBase 상속)
USTRUCT(BlueprintType)
struct FPetConversationData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 대화 내용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FText DialogueText;
	// 화자 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FText SpeakerName;
	// 펫 아이콘 (비워두면 컴포넌트 기본값 사용 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	class UTexture2D* PetIcon;
	// 함께 재생할 음성
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	USoundBase* VoiceAudio;
	// [추가사항] 함께 재생할 애니메이션 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	class UAnimMontage* MontageToPlay;
	// 다음 대화 ID == 만약 대화의 끝이라면 None으로 둠(칸 다 비우면 자동으로 None 처리됨)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FName NextDialogueID;
	// 펫 말고 다른 캐릭터가 말하는지 여부 == true면 펫이 앞으로 가서 마주보고 대화 X 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	bool bIsTalkToOtherCharacter = false;

	// 대화 선택지 사용 여부 == 주인공이 말하는 대화에만 사용 + 선택지는 사용시 무조건 2개여야 함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "선택지")
	bool bUseDialogueChoices = false;
	// [선택지 1] 버튼에 표시될 텍스트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "선택지", meta=(EditCondition="bUseDialogueChoices"))
	FText Choice1_Text;
	// [선택지 1] 눌렀을 때 이동할 다음 대화 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "선택지", meta=(EditCondition="bUseDialogueChoices"))
	FName Choice1_NextID;
	// [선택지 2] 버튼에 표시될 텍스트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "선택지", meta=(EditCondition="bUseDialogueChoices"))
	FText Choice2_Text;
	// [선택지 2] 눌렀을 때 이동할 다음 대화 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "선택지", meta=(EditCondition="bUseDialogueChoices"))
	FName Choice2_NextID;
	
	// CuteWhale 에셋 전용! -1 이면 작동 안함 -- 0 ~ 25, 0은 디폴트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CuteWhale전용")
	int32 CuteWhale_ColorIndex = -1;
	// CuteWhale 에셋 전용! -1 이면 작동 안함 -- 0 ~ 11, 0은 디폴트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CuteWhale전용")
	int32 CuteWhale_FaceIndex = -1;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PET_API UPetTalkComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// 외부에서 이 함수만 호출하면 됨 (텍스트 + 오디오 옵션) -> 탐험시 아이템 감지, 전투 등 다양한 상황에서 사용
	void Travel_Say(const FPetConversationData DialogueData, float Duration = 5.0f);
	// 데이터 테이블에서 랜덤한 행 데이터를 가져옴
	bool GetRandomDialogueFromTable(UDataTable* DataTable, FPetConversationData& OutData);
	// 입력 바인딩
	void BindInputForPet();
	
	// 탐험시 자막 위젯 클래스 (블루프린트에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TSubclassOf<class UTravelSubtitle> TravelSubtitleClass;
	UPROPERTY()
	UTravelSubtitle* TravelSubtitleInstance;
	// 탐사중 Follow -> Battle 상태 전환 대사 데이터
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UDataTable* Travel_FollowToBattleDialogue;
	// 탐사중 보스전 시작 대사 데이터
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UDataTable* Travel_BattleStartBoss;
	// 탐사중 Battle -> Follow 상태 전환 대사 데이터
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UDataTable* Travel_BattleToFollowDialogue;
	// 탐사중 보스전 종료 대사 데이터
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UDataTable* Travel_BattleEndBoss;
	// 탐사중 아이템 발견 대사 데이터 목록 
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UDataTable* TravelItemDetectDataTable;
	// 이미 발견해서 대사를 출력한 아이템 목록
	TArray<AActor*> DetectedItemHistory;
	// 목록 초기화용 타이머 핸들
	FTimerHandle HistoryResetTimerHandle;
	// 목록 초기화 함수
	void ResetDetectedItemHistory();
	
	// 대화 데이터 테이블
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UDataTable* BigConversationDataTable;
	// 탐험시 대화 데이터 테이블
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UDataTable* TravelSmallConversationDataTable;
	// 대화 자막 클래스
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TSubclassOf<class UConversationSubtitle> ConversationSubtitleClass;
	UPROPERTY()
	UConversationSubtitle* ConversationSubtitleInstance;
	// 대화 종료 함수
	UFUNCTION()
	void EndConversation();
	// 대화 지속 시간 타이머 핸들
	FTimerHandle ConversationTimerHandle;
	// 탐험용 연속 대화 타이머 핸들
	FTimerHandle TravelSmallConversationTimerHandle;
	// 현재 재생 중인 음성 오디오 컴포넌트 (필요시 중지용)
	UPROPERTY()
	UAudioComponent* CurrentConversationVoiceAudioComponent = nullptr;
	// 탐험시 재생 중인 음성 오디오 컴포넌트 (필요시 중지용)
	UPROPERTY()
	UAudioComponent* CurrentFollowVoiceAudioComponent = nullptr;
	// 현재 대화가 끝나면 넘어갈 다음 대화 ID 저장용
	FName NextDialogueID;
	// 대화 스킵 입력 액션
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "자체설정")
	class UInputAction* SkipDialogueAction;
	// 입력 바인딩 중복 방지를 위한 플래그
	bool bIsInputBound = false;
	// 대화 중 입력 처리를 위한 InputComponent
	UPROPERTY()
	UInputComponent* ConversationInputComponent;
	// 아이템 발견 시 생성하여 날려보낼 핑 액터 클래스 (블루프린트 할당용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	TSubclassOf<class APingActor> PingActorClass;
	// 핑 액터를 생성하고 목표 지점으로 이동시키는 내부 함수
	void SpawnItemPingEffectAtLocation(const FVector& TargetLocation, const FVector& PingSpawnLocation);


	// 대화 로그창 보이게 하는 함수
	UFUNCTION()
	void OnPressedLogButton();
	//대화 로그 창에 대화 내용 추가 함수
	void AddDialogueToConversationLog(const FText& SpeakerName, const FText& DialogueText, bool bIsSelection = false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	TSubclassOf<class UDialogueEntry> ConversationLogEntryClass;

	// 선택지 버튼 입력 처리 함수
	UFUNCTION()
	void OnDialogueChoiceSelect_0();
	UFUNCTION()
	void OnDialogueChoiceSelect_1();
	FName PendingChoice1_ID;
	FName PendingChoice2_ID;
	FText SpeakerName_Text;
	FText PendingChoice1_Text;
	FText PendingChoice2_Text;
	
public:
	UPetTalkComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	// 외부에서 아이템 발견시 호출 -> 랜덤하게 대사를 뽐아서 Travel_Say 호출
	UFUNCTION(BlueprintCallable)
	void Travel_ItemDetect(AActor* DetectedItem, const FVector& PingSpawnLocation);
	// 외부에서 Follow -> Battle 상태 전환 대사 호출
	UFUNCTION(BlueprintCallable)
	void Travel_FollowToBattle( bool bBoss );
	// 외부에서 Battle -> Follow 상태 전환 대사 호출
	UFUNCTION(BlueprintCallable)
	void Travel_BattleToFollow( bool bBoss );
	// 탐험시 대사. 다이얼로그 ID로 호출
	UFUNCTION(BlueprintCallable)
	void Travel_StartSmallConversation(FName DialogueID);
	
	// [추가] 대화 중에 교체할 컨텍스트 (빈 컨텍스트 혹은 스킵 키만 포함)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	class UInputMappingContext* ConversationMappingContext;
	
	// 대화 시작 함수
	UFUNCTION(BlueprintCallable)
	void StartConversation(FName DialogueID);
	// 외부(PlayerController 등)에서 키 입력 시 호출할 함수
	UFUNCTION(BlueprintCallable)
	void SkipCurrentDialogue();
	
	// 대화 종료시 호출될 델리게이트 인스턴스
	UPROPERTY(BlueprintAssignable)
	FOnConversationEnded OnConversationEnded;

	UFUNCTION(BlueprintCallable)
	bool ReturnWhoTalk(bool bIsTalkToOtherCharacter) { return bIsTalkToOtherCharacter; }

	// 대화 로그창 스크롤 바 초기화함수
	void ResetConversationLogScrollBox();
	// 대화 타이머 다시 시작
	UFUNCTION()
	void RestartConversationTimerHandle();
};

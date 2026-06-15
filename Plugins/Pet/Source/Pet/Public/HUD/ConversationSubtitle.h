#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConversationSubtitle.generated.h"


// 버튼 클릭 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSubtitleActionDelegate);

UCLASS()
class PET_API UConversationSubtitle : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	// 캐릭터 이름을 표시하는 텍스트
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Text_Name;

	// 대화 내용을 표시하는 텍스트 (기존 Text_Log 대체)
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Text_Dialogue;

	UPROPERTY(meta = (BindWidget))
	class UButton* SkipButton;
	UPROPERTY(meta = (BindWidget))
	class UButton* LogButton;
	
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DialogueChoiceText_0;
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DialogueChoiceText_1;
	UPROPERTY(meta = (BindWidget))
	class UButton* DialogueChoiceButton_0;
	UPROPERTY(meta = (BindWidget))
	class UButton* DialogueChoiceButton_1;
	

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* FadeInAnim;
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* FadeOutAnim;

public:
	// 대화 자막 설정 함수
	UFUNCTION(BlueprintCallable)
	void SetConversationSubtitle(const FText& InName, const FText& InDialogue);
	UPROPERTY()
	class UConversationLog* LogWidgetInstance;
	
	FTimerHandle FadeOutTimerHandle;
	
	UFUNCTION(BlueprintCallable)
	void PlayFadeInAnimation();
	UFUNCTION(BlueprintCallable)
	void PlayFadeOutAnimation();
	void OnFadeOutFinished();


	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TSubclassOf<UUserWidget> ConversationLogWidgetClass;
	
	UFUNCTION(BlueprintCallable)
	void OnPressedSkipButton();
	FOnSubtitleActionDelegate OnSkipClicked;
	
	UFUNCTION(BlueprintCallable)
	void OnPressedLogButton();
	FOnSubtitleActionDelegate OnLogClicked;

	UFUNCTION(BlueprintCallable)
	void OnPressedConversationButton();
	FOnSubtitleActionDelegate OnConversationButtonClicked;
	
	// 0번은 기본 대사창, 1번은 선택지 대사창
	UPROPERTY(meta = (BindWidget))
	class UWidgetSwitcher* WidgetSwitcher;
	
	// 대화창 클릭시 다음 대화로 넘어가기
	UPROPERTY(meta = (BindWidget))
	class UButton* ConversationButton;
	
	void SetupChoiceDialogueText(const FText& Choice0Text, const FText& Choice1Text);
	UFUNCTION(BlueprintCallable)
	void OnClickDialogueChoice_0();
	FOnSubtitleActionDelegate OnDialogueChoice_0Clicked;
	UFUNCTION(BlueprintCallable)
	void OnClickDialogueChoice_1();
	FOnSubtitleActionDelegate OnDialogueChoice_1Clicked;
	
};

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConversationLog.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLogCloseClicked); // 델리게이트 타입 선언


UCLASS()
class PET_API UConversationLog : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta = (BindWidget))
	class UButton* QutiButton;
	
public:
	UPROPERTY(meta = (BindWidget))
	class UScrollBox* ScrollBox;

	FOnLogCloseClicked OnCloseClicked;
	
	UFUNCTION(BlueprintCallable)
	void OnPressedQuitButton();
	
};

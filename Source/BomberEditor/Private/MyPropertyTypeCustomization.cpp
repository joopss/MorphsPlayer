// Copyright 2021 Yevhenii Selivanov.

#include "MyPropertyTypeCustomization.h"
//---
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "SSearchableComboBox.h"

typedef Super ThisClass;

// Get property from handle
FProperty* FPropertyData::GetProperty() const
{
	return PropertyHandle ? PropertyHandle->GetProperty() : nullptr;
}

// Get property name by handle
FName FPropertyData::GetPropertyNameFromHandle() const
{
	const FProperty* CurrentProperty = GetProperty();
	return CurrentProperty ? CurrentProperty->GetFName() : NAME_None;
}

// Get FName value by property handle
FName FPropertyData::GetPropertyValueFromHandle() const
{
	FName ValueName = NAME_None;
	if (PropertyHandle.IsValid())
	{
		FString ValueString;
		PropertyHandle->GetValueAsDisplayString(/*Out*/ValueString);
		ValueName = *ValueString;
	}
	return ValueName;
}

// Set FName value by property handle
template <typename T>
void FPropertyData::SetPropertyValueToHandle(const T& NewValue)
{
	if (PropertyHandle.IsValid())
	{
		PropertyHandle->SetValue(NewValue);
	}
}

// Called when the header of the property (the row in the details panel where the property is shown)
void FMyPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Use default the header details panel
	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			PropertyHandle->CreatePropertyValueWidget()
		];
}

// Called when the children of the property should be customized or extra rows added.
void FMyPropertyTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Find outer
	TArray<UObject*> OuterObjects;
	PropertyHandle/*ref*/->GetOuterObjects(OuterObjects);
	MyPropertyOuterInternal = OuterObjects.IsValidIndex(0) ? OuterObjects[0] : nullptr;

	// Bind to delegate
	PropertyHandle/*ref*/->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &ThisClass::RefreshCustomProperty));

	uint32 NumChildren;
	PropertyHandle/*ref*/->GetNumChildren(NumChildren);
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		FPropertyData PropertyData;
		PropertyData.PropertyHandle = PropertyHandle/*ref*/->GetChildHandle(ChildIndex);
		PropertyData.PropertyName = PropertyData.GetPropertyNameFromHandle();
		PropertyData.PropertyValue = PropertyData.GetPropertyValueFromHandle();
		OnCustomizeChildren(ChildBuilder, PropertyData);
	}
}

// Set the FName value into the property
void FMyPropertyTypeCustomization::SetCustomPropertyValue(FName Value)
{
	const FString StringToSet = Value.ToString();

	// Set value into property
	CustomProperty.PropertyValue = Value;
	CustomProperty.SetPropertyValueToHandle(Value);

	if (const TSharedPtr<STextBlock>& RowTextWidget = RowTextWidgetInternal.Pin())
	{
		// Update value on displayed widget
		RowTextWidget->SetText(FText::FromString(StringToSet));
	}
}

// Set true to activate property, false to grey out it (read-only)
void FMyPropertyTypeCustomization::SetCustomPropertyEnabled(bool bEnabled)
{
	const bool bIsAllowedEnableCustomProperty = IsAllowedEnableCustomProperty();
	if (bEnabled && !bIsAllowedEnableCustomProperty)
	{
		// Enable is forbidden
		return;
	}

	if (const TSharedPtr<SSearchableComboBox>& SearchableComboBox = SearchableComboBoxInternal.Pin())
	{
		SearchableComboBox->SetEnabled(bEnabled);
	}

	CustomProperty.bIsEnabled = bEnabled;
}

// Is called for each property on building its row
void FMyPropertyTypeCustomization::OnCustomizeChildren(IDetailChildrenBuilder& ChildBuilder, FPropertyData& PropertyData)
{
	if (!ensureMsgf(PropertyData.PropertyHandle, TEXT("ASSERT: 'PropertyData.PropertyHandle' is not valid")))
	{
		return;
	}

	if (PropertyData.PropertyName != CustomProperty.PropertyName)
	{
		// Add each another property to the Details Panel without customization
		IDetailPropertyRow& AddedRow = ChildBuilder.AddProperty(PropertyData.PropertyHandle.ToSharedRef())
		                                           .ShouldAutoExpand(true)
		                                           .IsEnabled(PropertyData.bIsEnabled)
		                                           .Visibility(PropertyData.Visibility);
		DefaultPropertiesData.Emplace(PropertyData);
		return;
	}

	// --- Is custom property ---

	CustomProperty = PropertyData;

	// Add as searchable combo box by default
	AddCustomPropertyRow(PropertyData.PropertyHandle->GetPropertyDisplayName(), ChildBuilder);
}

// Will add the default searchable combo box
void FMyPropertyTypeCustomization::AddCustomPropertyRow(const FText& PropertyDisplayText, IDetailChildrenBuilder& ChildBuilder)
{
	RefreshCustomProperty();

	// Will add the searchable combo box by default
	const TSharedRef<STextBlock> TextRowWidgetRef =
		SNew(STextBlock)
		.Text(GetCustomPropertyValue());
	RowTextWidgetInternal = TextRowWidgetRef;

	const TSharedRef<SSearchableComboBox> SearchableComboBoxRef =
		SNew(SSearchableComboBox)
                .OptionsSource(&SearchableComboBoxValuesInternal)
                .OnGenerateWidget_Lambda([](const TSharedPtr<FString> InItem) -> TSharedRef<SWidget>
		                         {
			                         return SNew(STextBlock).Text(FText::FromString(*InItem));
		                         })
                .OnSelectionChanged(this, &FMyPropertyTypeCustomization::OnCustomPropertyChosen)
                .ContentPadding(2.f)
                .MaxListHeight(200.f)
				.IsEnabled(CustomProperty.bIsEnabled)
				.Content()
		[
			TextRowWidgetRef
		];
	SearchableComboBoxInternal = SearchableComboBoxRef;

	ChildBuilder.AddCustomRow(PropertyDisplayText)
	            .Visibility(CustomProperty.Visibility)
	            .NameContent()
		[
			SNew(STextBlock)
               .Text(PropertyDisplayText)
               .Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			  .AutoHeight()
			  .VAlign(VAlign_Fill)
			  .Padding(0.f)
			[
				SearchableComboBoxRef
			]
		];
}

//Set new values for the list of selectable members
void FMyPropertyTypeCustomization::RefreshCustomProperty()
{
	if (const TSharedPtr<SSearchableComboBox>& SearchableComboBox = SearchableComboBoxInternal.Pin())
	{
		SearchableComboBox->RefreshOptions();
	}
}

// Is called to deactivate custom property
void FMyPropertyTypeCustomization::InvalidateCustomProperty()
{
	SetCustomPropertyEnabled(false);

	SetCustomPropertyValue(NAME_None);
}

// Called when the children of the property should be customized or extra rows added
void FMyPropertyTypeCustomization::OnCustomPropertyChosen(TSharedPtr<FString> SelectedStringPtr, ESelectInfo::Type SelectInfo)
{
	if (const FString* SelectedString = SelectedStringPtr.Get())
	{
		SetCustomPropertyValue(**SelectedString);
	}
}

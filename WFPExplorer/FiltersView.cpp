#include "pch.h"
#include "FiltersView.h"
#include <WFPEngine.h>
#include "StringHelper.h"
#include <SortHelper.h>
#include "resource.h"
#include "FilterGeneralPage.h"
#include "FilterConditionsPage.h"
#include "WFPHelper.h"
#include <ResizablePropertySheet.h>
#include "AppSettings.h"

CFiltersView::CFiltersView(IMainFrame* frame, WFPEngine& engine) : CFrameView(frame), m_Engine(engine) {
}

void CFiltersView::SetLayer(GUID const& layer) {
	m_Layer = layer;
}

void CFiltersView::Refresh() {
	CWaitCursor wait;
	m_Filters = m_Engine.EnumFilters<FilterInfo>(m_Layer);
	Sort(m_List);
	m_List.SetItemCountEx((int)m_Filters.size(), LVSICF_NOSCROLL);
	Frame()->SetStatusText(3, std::format(L"{} Filters", m_Filters.size()).c_str());
}

CString CFiltersView::GetColumnText(HWND, int row, int col) {
	auto& info = m_Filters[row];
	switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Key: return StringHelper::GuidToString(info.FilterKey);
		case ColumnType::Name: return info.Name.c_str();
		case ColumnType::Desc: return info.Desc.c_str();
		case ColumnType::Id: return std::to_wstring(info.FilterId).c_str();
		case ColumnType::ConditionCount: return std::to_wstring(info.ConditionCount).c_str();
		case ColumnType::LayerKey: return StringHelper::GuidToString(info.LayerKey);
		case ColumnType::SubLayerKey: return StringHelper::GuidToString(info.SubLayerKey);
		case ColumnType::Weight: return StringHelper::WFPValueToString(info.Weight, true);
		case ColumnType::Action: return StringHelper::WFPFilterActionTypeToString(info.Action.Type);
		case ColumnType::ProviderData: return info.ProviderDataSize == 0 ? L"" : std::format(L"{} Bytes", info.ProviderDataSize).c_str();
		case ColumnType::ActionKey:
			if (info.FilterAction.IsEmpty()) {
				if (info.Action.CalloutKey == GUID_NULL)
					info.FilterAction = L"(None)";
				else {
					auto filter = m_Engine.GetFilterByKey(info.Action.FilterType);
					if (filter)
						info.FilterAction = filter->Name.c_str();
					else {
						auto callout = m_Engine.GetCalloutByKey(info.Action.CalloutKey);
						if (callout)
							info.FilterAction = callout->Name.c_str();
					}
				}
			}
			return info.FilterAction;

		case ColumnType::Flags:
			if (info.Flags == WFPFilterFlags::None)
				return L"0";
			return std::format(L"0x{:02X} ({})", (UINT32)info.Flags, 
				(PCWSTR)StringHelper::WFPFilterFlagsToString(info.Flags)).c_str();
		case ColumnType::EffectiveWeight: return StringHelper::WFPValueToString(info.EffectiveWeight, true);
		case ColumnType::ProviderName: return GetProviderName(info);
		case ColumnType::Layer: return GetLayerName(info);
		case ColumnType::SubLayer: return GetSublayerName(info);
	}
	return CString();
}

void CFiltersView::UpdateUI() {
	auto& ui = Frame()->UI();
	ui.UIEnable(ID_EDIT_PROPERTIES, m_List.GetSelectedCount() == 1);
}

CString const& CFiltersView::GetProviderName(FilterInfo& info) const {
	if (info.ProviderName.IsEmpty() && info.ProviderKey != GUID_NULL)
		info.ProviderName = WFPHelper::GetProviderName(m_Engine, info.ProviderKey);
	return info.ProviderName;
}

CString const& CFiltersView::GetLayerName(FilterInfo& info) const {
	if (info.Layer.IsEmpty() && info.LayerKey != GUID_NULL) {
		auto layer = m_Engine.GetLayerByKey(info.LayerKey);
		if (layer && !layer.value().Name.empty() && layer.value().Name[0] != L'@')
			info.Layer = layer.value().Name.c_str();
		else
			info.Layer = StringHelper::GuidToString(info.LayerKey);
	}
	return info.Layer;
}

CString const& CFiltersView::GetSublayerName(FilterInfo& info) const {
	if (info.SubLayer.IsEmpty() && info.SubLayerKey != GUID_NULL) {
		auto layer = m_Engine.GetSublayerByKey(info.SubLayerKey);
		if (layer && !layer.value().Name.empty() && layer.value().Name[0] != L'@')
			info.SubLayer = layer.value().Name.c_str();
		else
			info.SubLayer = StringHelper::GuidToString(info.LayerKey);
	}
	return info.SubLayer;
}

LRESULT CFiltersView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | LVS_OWNERDATA | LVS_REPORT | LVS_SINGLESEL);
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_HEADERDRAGDROP);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Filter Key", 0, 300, ColumnType::Key);
	cm->AddColumn(L"Weight", LVCFMT_RIGHT, 140, ColumnType::Weight);
	cm->AddColumn(L"Effective Weight", LVCFMT_RIGHT, 140, ColumnType::EffectiveWeight);
	cm->AddColumn(L"Conditions", LVCFMT_RIGHT, 70, ColumnType::ConditionCount);
	cm->AddColumn(L"Action", LVCFMT_LEFT, 110, ColumnType::Action);
	cm->AddColumn(L"Action Filter/Callout", LVCFMT_LEFT, 120, ColumnType::ActionKey);
	cm->AddColumn(L"Flags", LVCFMT_LEFT, 150, ColumnType::Flags);
	cm->AddColumn(L"Provider Data", LVCFMT_RIGHT, 100, ColumnType::ProviderData);
	cm->AddColumn(L"Filter Name", 0, 180, ColumnType::Name);
	cm->AddColumn(L"Description", 0, 180, ColumnType::Desc);
	cm->AddColumn(L"Provider", 0, 240, ColumnType::ProviderName);
	cm->AddColumn(L"Layer", LVCFMT_LEFT, 250, ColumnType::Layer);
	cm->AddColumn(L"Sublayer", LVCFMT_LEFT, 250, ColumnType::SubLayer);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
	UINT icons[] = { IDI_FILTER, IDI_FILTER_PERMIT, IDI_FILTER_BLOCK, IDI_FILTER_REFRESH };
	for(auto icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_List.SetImageList(images, LVSIL_SMALL);

	return 0;
}

LRESULT CFiltersView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}

LRESULT CFiltersView::OnProperties(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_List.GetSelectedIndex() >= 0);
	auto& filter = m_Filters[m_List.GetSelectedIndex()];
	WFPHelper::ShowFilterProperties(m_Engine, filter);

	return 0;
}

LRESULT CFiltersView::OnActivate(UINT, WPARAM activate, LPARAM, BOOL&) {
	if(activate)
		UpdateUI();
	return 0;
}

void CFiltersView::DoSort(SortInfo const* si) {
	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto asc = si->SortAscending;

	auto compare = [&](auto& f1, auto& f2) {
		switch (col) {
			case ColumnType::Key: return SortHelper::Sort(StringHelper::GuidToString(f1.FilterKey), StringHelper::GuidToString(f2.FilterKey), asc);
			case ColumnType::Name: return SortHelper::Sort(f1.Name, f2.Name, asc);
			case ColumnType::Desc: return SortHelper::Sort(f1.Desc, f2.Desc, asc);
			case ColumnType::Action: return SortHelper::Sort(f1.Action.Type, f2.Action.Type, asc);
			case ColumnType::ActionKey: return SortHelper::Sort(f1.FilterAction, f2.FilterAction, asc);
			case ColumnType::Flags: return SortHelper::Sort(f1.Flags, f2.Flags, asc);
			case ColumnType::ProviderName: return SortHelper::Sort(GetProviderName(f1), GetProviderName(f2), asc);
			case ColumnType::Weight: return SortHelper::Sort(f1.Weight.uint8, f2.Weight.uint8, asc);
			case ColumnType::EffectiveWeight: return SortHelper::Sort(f1.EffectiveWeight.uint64, f2.EffectiveWeight.uint64, asc);
			case ColumnType::Layer: return SortHelper::Sort(GetLayerName(f1), GetLayerName(f2), asc);
			case ColumnType::SubLayer: return SortHelper::Sort(GetSublayerName(f1), GetSublayerName(f2), asc);
			case ColumnType::ConditionCount: return SortHelper::Sort(f1.ConditionCount, f2.ConditionCount, asc);
			case ColumnType::ProviderData: return SortHelper::Sort(f1.ProviderDataSize, f2.ProviderDataSize, asc);
		}
		return false;
	};
	std::ranges::sort(m_Filters, compare);
}

int CFiltersView::GetSaveColumnRange(HWND, int&) const {
	return 1;
}

int CFiltersView::GetRowImage(HWND, int row, int col) const {
	auto& filter = m_Filters[row];
	switch (filter.Action.Type) {
		case WFPActionType::Block: return 2;
		case WFPActionType::Permit: return 1;
		case WFPActionType::Continue: return 3;
	}
	return 0;
}

void CFiltersView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	if((newState & LVIS_SELECTED) || (oldState & LVIS_SELECTED))
		UpdateUI();
}

bool CFiltersView::OnDoubleClickList(HWND, int row, int col, POINT const& pt) {
	LRESULT result;
	return ProcessWindowMessage(m_hWnd, WM_COMMAND, ID_EDIT_PROPERTIES, 0, result, 1);
}

DWORD CFiltersView::OnPrePaint(int, LPNMCUSTOMDRAW cd) {
	return cd->hdr.hwndFrom == m_List ? CDRF_NOTIFYITEMDRAW : 0;
}

DWORD CFiltersView::OnItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	return cd->hdr.hwndFrom == m_List ? CDRF_NOTIFYSUBITEMDRAW : 0;
}

DWORD CFiltersView::OnSubItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(lv->iSubItem);
	if (col == ColumnType::Key || col == ColumnType::Weight || col == ColumnType::EffectiveWeight) {
		::SelectObject(cd->hdc, Frame()->GetMonoFont());
	}
	else {
		::SelectObject(cd->hdc, m_List.GetFont());
	}
	return CDRF_NEWFONT;
}

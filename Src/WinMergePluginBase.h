#include <oleauto.h>
#include <string>
#include <map>

class WinMergePluginBase : public IDispatch, public ITypeInfo
{
public:
	enum
	{
		DISPID_UnpackFile,
		DISPID_PackFile,
		DISPID_ShowSettingsDialog,
		DISPID_PluginFileFilters,
		DISPID_PluginIsAutomatic,
		DISPID_PluginDescription,
		DISPID_PluginEvent,
	};
	struct MemberInfo { std::wstring name; DISPID id; int flags; };

	WinMergePluginBase() : m_nRef(0)
	{
		static const MemberInfo memberInfo[] =
		{
			{ L"UnpackFile",         DISPID_UnpackFile,         DISPATCH_METHOD },
			{ L"PackFile",           DISPID_PackFile ,          DISPATCH_METHOD },
			{ L"ShowSettingsDialog", DISPID_ShowSettingsDialog, DISPATCH_METHOD },
			{ L"PluginFileFilters",  DISPID_PluginFileFilters,  DISPATCH_PROPERTYGET },
			{ L"PluginIsAutomatic",  DISPID_PluginIsAutomatic,  DISPATCH_PROPERTYGET },
			{ L"PluginDescription",  DISPID_PluginDescription,  DISPATCH_PROPERTYGET },
			{ L"PluginEvent",        DISPID_PluginEvent,        DISPATCH_PROPERTYGET },
		};
		for (auto item : memberInfo)
		{
			m_memberInfo.push_back(item);
			m_mapNameToIndex.emplace(item.name, item.id);
		}
	}

	virtual ~WinMergePluginBase() {}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		return E_NOTIMPL;
	}

	ULONG STDMETHODCALLTYPE AddRef(void) override
	{
		return ++m_nRef;
	}

	ULONG STDMETHODCALLTYPE Release(void) override
	{
		if (--m_nRef == 0)
		{
			delete this;
			return 0;
		}
		return m_nRef;
	}

	HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* pctinfo) override
	{
		*pctinfo = static_cast<UINT>(m_memberInfo.size());
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override
	{
		*ppTInfo = this;
		(*ppTInfo)->AddRef();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) override
	{
		for (unsigned i = 0; i < cNames; ++i)
		{
			auto it = m_mapNameToIndex.find(rgszNames[i]);
			if (it == m_mapNameToIndex.end())
				return DISP_E_UNKNOWNNAME;
			rgDispId[i] = it->second;
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) override
	{
		if (!pDispParams)
			return DISP_E_BADVARTYPE;
		HRESULT hr = E_NOTIMPL;
		if (wFlags == DISPATCH_METHOD)
		{
			switch (dispIdMember)
			{
			case DISPID_UnpackFile:
			{
				BSTR fileSrc = pDispParams->rgvarg[3].bstrVal;
				BSTR fileDst = pDispParams->rgvarg[2].bstrVal;
				VARIANT_BOOL* pbChanged = pDispParams->rgvarg[1].pboolVal;
				INT* pSubcode = &pDispParams->rgvarg[0].intVal;
				VARIANT_BOOL* pbSuccess = &pVarResult->boolVal;
				hr = UnpackFile(fileSrc, fileDst, pbChanged, pSubcode, pbSuccess);
				break;
			}
			case DISPID_PackFile:
			{
				BSTR fileSrc = pDispParams->rgvarg[3].bstrVal;
				BSTR fileDst = pDispParams->rgvarg[2].bstrVal;
				VARIANT_BOOL* pbChanged = pDispParams->rgvarg[1].pboolVal;
				INT subcode = pDispParams->rgvarg[0].intVal;
				VARIANT_BOOL* pbSuccess = &pVarResult->boolVal;
				hr = PackFile(fileSrc, fileDst, pbChanged, subcode, pbSuccess);
				break;
			}
			case DISPID_ShowSettingsDialog:
				hr = ShowSettingsDialog(&pVarResult->boolVal);
				break;
			}
		}
		else if (wFlags == DISPATCH_PROPERTYGET)
		{
			switch (dispIdMember)
			{
			case DISPID_PluginFileFilters:
				pVarResult->vt = VT_BSTR;
				hr = get_PluginFileFilters(&pVarResult->bstrVal);
				break;
			case DISPID_PluginIsAutomatic:
				pVarResult->vt = VT_BOOL;
				hr = get_PluginIsAutomatic(&pVarResult->boolVal);
				break;
			case DISPID_PluginDescription:
				pVarResult->vt = VT_BSTR;
				hr = get_PluginDescription(&pVarResult->bstrVal);
				break;
			case DISPID_PluginEvent:
				pVarResult->vt = VT_BSTR;
				hr = get_PluginEvent(&pVarResult->bstrVal);
				break;
			}
		}
		return hr;
	}

	HRESULT STDMETHODCALLTYPE GetTypeAttr(TYPEATTR** ppTypeAttr) override
	{
		auto* pTypeAttr = new TYPEATTR();
		pTypeAttr->cFuncs = static_cast<WORD>(m_memberInfo.size());
		*ppTypeAttr = pTypeAttr;
		return S_OK;
	}
	
	HRESULT STDMETHODCALLTYPE GetTypeComp(ITypeComp** ppTComp) override
	{
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE GetFuncDesc(UINT index, FUNCDESC** ppFuncDesc) override
	{
		if (index >= m_memberInfo.size())
			return E_INVALIDARG;
		auto* pFuncDesc = new FUNCDESC();
		pFuncDesc->invkind = static_cast<INVOKEKIND>(m_memberInfo[index].flags);
		pFuncDesc->wFuncFlags = 0;
		pFuncDesc->memid = index;
		*ppFuncDesc = pFuncDesc;
		return S_OK;
	}
	
	HRESULT STDMETHODCALLTYPE GetVarDesc(UINT index, VARDESC** ppVarDesc) override
	{
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE GetNames(MEMBERID memid, BSTR *rgBstrNames, UINT cMaxNames, UINT *pcNames) override
	{
		if (memid >= m_memberInfo.size())
			return E_INVALIDARG;
		*rgBstrNames = SysAllocString(m_memberInfo[memid].name.c_str());
		*pcNames = 1;
		return S_OK;
	}
	
	HRESULT STDMETHODCALLTYPE GetRefTypeOfImplType(UINT index, HREFTYPE *pRefType) override
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE GetImplTypeFlags(UINT index, INT *pImplTypeFlags) override
	{
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE GetIDsOfNames(LPOLESTR* rgszNames, UINT cNames, MEMBERID* pMemId) override
	{
		return GetIDsOfNames(IID_NULL, rgszNames, cNames, 0, pMemId);
	}
	
	HRESULT STDMETHODCALLTYPE Invoke(PVOID pvInstance, MEMBERID memid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr) override
	{
		return reinterpret_cast<IDispatch*>(pvInstance)->Invoke(memid, IID_NULL, 0, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
	}
	
	HRESULT STDMETHODCALLTYPE GetDocumentation(MEMBERID memid, BSTR *pBstrName, BSTR *pBstrDocString, DWORD *pdwHelpContext, BSTR *pBstrHelpFile) override
	{
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE GetDllEntry(MEMBERID memid, INVOKEKIND invKind, BSTR *pBstrDllName, BSTR *pBstrName, WORD *pwOrdinal)
	{
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE GetRefTypeInfo(HREFTYPE hRefType, ITypeInfo **ppTInfo) override
	{
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE AddressOfMember(MEMBERID memid, INVOKEKIND invKind, PVOID *ppv) override
	{
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *pUnkOuter, REFIID riid, PVOID *ppvObj) override
	{
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE GetMops(MEMBERID memid, BSTR *pBstrMops) override
	{
		return E_NOTIMPL;
	}
	
	HRESULT STDMETHODCALLTYPE GetContainingTypeLib(ITypeLib **ppTLib, UINT *pIndex) override
	{
		return E_NOTIMPL;
	}
	
	void STDMETHODCALLTYPE ReleaseTypeAttr(TYPEATTR *pTypeAttr) override
	{
		delete pTypeAttr;
	}
	
	void STDMETHODCALLTYPE ReleaseFuncDesc(FUNCDESC *pFuncDesc) override
	{
		delete pFuncDesc;
	}
	
	void STDMETHODCALLTYPE ReleaseVarDesc(VARDESC *pVarDesc) override
	{
	}
	
	virtual HRESULT STDMETHODCALLTYPE PackFile(BSTR fileSrc, BSTR fileDst, VARIANT_BOOL* pbChanged, INT subcode, VARIANT_BOOL* pbSuccess)
	{
		*pbSuccess = VARIANT_FALSE;
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE UnpackFile(BSTR fileSrc, BSTR fileDst, VARIANT_BOOL* pbChanged, INT* pSubcode, VARIANT_BOOL* pbSuccess)
	{
		*pbSuccess = VARIANT_FALSE;
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE ShowSettingsDialog(VARIANT_BOOL* pbHandled)
	{
		*pbHandled = VARIANT_FALSE;
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE get_PluginIsAutomatic(VARIANT_BOOL* pVal)
	{
		*pVal = m_bIsAutomatic ? -1 : 0;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_PluginFileFilters(BSTR* pVal)
	{
		*pVal = SysAllocString(m_sFileFilters.c_str());
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_PluginDescription(BSTR* pVal)
	{
		*pVal = SysAllocString(m_sDescription.c_str());
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_PluginEvent(BSTR* pVal)
	{
		*pVal = SysAllocString(m_sEvent.c_str());
		return S_OK;
	}

protected:
	int m_nRef;
	std::map<std::wstring, int> m_mapNameToIndex;
	std::vector<MemberInfo> m_memberInfo;
	std::wstring m_sEvent;
	std::wstring m_sDescription;
	std::wstring m_sFileFilters;
	bool m_bIsAutomatic;
};


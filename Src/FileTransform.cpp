/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/**
 *  @file FileTransform.cpp
 *
 *  @brief Implementation of file transformations
 */ 

#include "pch.h"
#include "FileTransform.h"
#include <vector>
#include <Poco/Exception.h>
#include <Poco/Mutex.h>
#include "Plugins.h"
#include "multiformatText.h"
#include "Environment.h"
#include "TFile.h"
#include "paths.h"
#include "MergeApp.h"

using Poco::Exception;

static Poco::FastMutex g_mutex;

////////////////////////////////////////////////////////////////////////////////
// transformations : packing unpacking


std::vector<PluginForFile::PipelineItem> PluginForFile::ParsePluginPipeline(String& errorMessage) const
{
	return ParsePluginPipeline(m_PluginPipeline, errorMessage);
}

std::vector<PluginForFile::PipelineItem> PluginForFile::ParsePluginPipeline(const String& pluginPipeline, String& errorMessage)
{
	std::vector<PluginForFile::PipelineItem> result;
	bool inQuotes = false;
	TCHAR quoteChar = 0;
	std::vector<String> args;
	String token, name;
	errorMessage.clear();
	const TCHAR* p = pluginPipeline.c_str();
	while (_istspace(*p)) p++;
	while (*p)
	{
		TCHAR sep = 0;
		while (*p)
		{
			if (!inQuotes)
			{
				if (*p == '"' || *p == '\'')
				{
					inQuotes = true;
					quoteChar = *p;
				}
				else if (_istspace(*p))
				{
					sep = *p;
					break;
				}
				else if (*p == '|')
				{
					sep = *p;
					break;
				}
				else
					token += *p;
			}
			else
			{
				if (*p == quoteChar)
				{
					if (*(p + 1) == quoteChar)
					{
						token += *p;
						++p;
					}
					else
					{
						inQuotes = false;
					}
				}
				else
				{
					token += *p;
				}
			}
			++p;
		}
		if (name.empty())
		{
			name = token;
		}
		else
		{
			args.push_back(token);
		}
		while (_istspace(*p)) p++;
		if (*p == '|')
			sep = *p;
		if (sep == '|')
			p++;
		token.clear();
		if (sep == '|' || !*p)
		{
			if (name.empty() || (sep == '|' && !*p))
			{
				errorMessage = strutils::format_string1(_("Missing plugin name in plugin pipeline: %1"), pluginPipeline);
				break;
			}
			result.push_back({ name, args, quoteChar });
			name.clear();
			args.clear();
			quoteChar = 0;
		}
	};
	if (inQuotes)
		errorMessage = strutils::format_string1(_("Missing quotation mark in plugin pipeline: %1"), pluginPipeline);
	return result;
}

String PluginForFile::MakePluginPipeline(const std::vector<PluginForFile::PipelineItem>& list)
{
	int i = 0;
	String pipeline;
	for (const auto& [name, args, quoteChar] : list)
	{
		if (quoteChar && name.find_first_of(_T(" '\"")) != String::npos)
		{
			String nameQuoted = name;
			strutils::replace(nameQuoted, String(1, quoteChar), String(2, quoteChar));
			pipeline += strutils::format(_T("%c%s%c"), quoteChar, nameQuoted, quoteChar);
		}
		else
		{
			pipeline += name;
		}
		if (!args.empty())
		{
			for (const auto& arg : args)
			{
				if (quoteChar)
				{
					String argQuoted = arg;
					strutils::replace(argQuoted, String(1, quoteChar), String(2, quoteChar));
					pipeline += strutils::format(_T(" %c%s%c"), quoteChar, argQuoted, quoteChar);
				}
				else
				{
					pipeline += _T(" ") + arg;
				}
			}
		}
		if (i < list.size() - 1)
			pipeline += _T("|");
		i++;
	}
	return pipeline;
}

String PluginForFile::MakeArguments(const std::vector<String>& args, const std::vector<StringView>& variables)
{
	String newstr;
	int i = 0;
	for (const auto& arg : args)
	{
		String newarg;
		for (const TCHAR* p = arg.c_str(); *p; ++p)
		{
			if (*p == '%' && *(p + 1) != 0)
			{
				++p;
				TCHAR c = *p;
				if (c == '%')
				{
					newarg += '%';
				}
				else if (c >= '1' && c <= '9')
				{
					if ((c - '1') < variables.size())
						newarg += String{ variables[(c - '1')].data(), variables[(c - '1')].length() };
				}
				else
				{
					newarg += *(p - 1);
					newarg += c;
				}
			}
			else
			{
				newarg += *p;
			}
		}
		if (newarg.find_first_of(_T(" \"")) != String::npos)
		{
			strutils::replace(newarg, _T("\""), _T("\"\""));
			newstr += _T("\"") + newarg + _T("\"");
		}
		else
		{
			newstr += newarg;
		}
		if (i < args.size() - 1)
			newstr += ' ';
		i++;
	}
	return newstr;
}

bool PackingInfo::GetPackUnpackPlugin(const String& filteredFilenames, bool bReverse,
	std::vector<std::tuple<PluginInfo*, std::vector<String>, bool>>& plugins,
	String *pPluginPipelineResolved, String& errorMessage) const
{
	auto result = ParsePluginPipeline(errorMessage);
	if (!errorMessage.empty())
		return false;
	std::vector<PluginForFile::PipelineItem> pipelineResolved;
	for (auto& [pluginName, args, quoteChar] : result)
	{
		PluginInfo* plugin = nullptr;
		bool bWithFile = true;
		if (pluginName == _T("<None>") || pluginName == _("<None>"))
			;
		else if (pluginName == _T("<Automatic>") || pluginName == _("<Automatic>"))
		{
			plugin = CAllThreadsScripts::GetActiveSet()->GetAutomaticPluginByFilter(L"FILE_PACK_UNPACK", filteredFilenames);
			if (plugin == nullptr)
				plugin = CAllThreadsScripts::GetActiveSet()->GetAutomaticPluginByFilter(L"FILE_FOLDER_PACK_UNPACK", filteredFilenames);
			if (plugin == nullptr)
			{
				plugin = CAllThreadsScripts::GetActiveSet()->GetAutomaticPluginByFilter(L"BUFFER_PACK_UNPACK", filteredFilenames);
				bWithFile = false;
			}
		}
		else
		{
			plugin = CAllThreadsScripts::GetActiveSet()->GetPluginByName(L"FILE_PACK_UNPACK", pluginName);
			if (plugin == nullptr)
			{
				plugin = CAllThreadsScripts::GetActiveSet()->GetPluginByName(L"FILE_FOLDER_PACK_UNPACK", pluginName);
				if (plugin == nullptr)
				{
					plugin = CAllThreadsScripts::GetActiveSet()->GetPluginByName(L"BUFFER_PACK_UNPACK", pluginName);
					if (plugin == nullptr)
					{
						plugin = CAllThreadsScripts::GetActiveSet()->GetPluginByName(nullptr, pluginName);
						if (plugin == nullptr)
						{
							errorMessage = strutils::format_string1(_("Plugin not found or invalid: %1"), pluginName);
						}
						else
						{
							plugin = nullptr;
							errorMessage = strutils::format(_T("'%s' is not PACK_UNPACK plugin"), pluginName);
						}
						return false;
					}
					bWithFile = false;
				}
			}
		}
		if (plugin)
		{
			pipelineResolved.push_back({plugin->m_name, args, quoteChar });
			if (bReverse)
				plugins.insert(plugins.begin(), { plugin, args, bWithFile });
			else
				plugins.push_back({ plugin, args, bWithFile });
		}
	}
	if (pPluginPipelineResolved)
		*pPluginPipelineResolved = MakePluginPipeline(pipelineResolved);
	return true;
}

// known handler
bool PackingInfo::Packing(String & filepath, const std::vector<int>& handlerSubcodes, const std::vector<StringView>& variables) const
{
	// no handler : return true
	if (m_PluginPipeline.empty())
		return true;

	// control value
	String errorMessage;
	std::vector<std::tuple<PluginInfo*, std::vector<String>, bool>> plugins;
	if (!GetPackUnpackPlugin(_T(""), true, plugins, nullptr, errorMessage))
	{
		AppErrorMessageBox(errorMessage);
		return false;
	}

	auto itSubcode = handlerSubcodes.rbegin();
	for (auto& [plugin, args, bWithFile] : plugins)
	{
		bool bHandled = false;
		storageForPlugins bufferData;
		bufferData.SetDataFileAnsi(filepath);

		LPDISPATCH piScript = plugin->m_lpDispatch;
		Poco::FastMutex::ScopedLock lock(g_mutex);

		if (plugin->m_hasVariablesProperty)
		{
			if (!plugin::InvokePutPluginVariables(String(variables[0].data(), variables[0].length()), piScript))
				return false;
		}
		if (plugin->m_hasArgumentsProperty)
		{
			if (!plugin::InvokePutPluginArguments(args.empty() ? plugin->m_arguments : MakeArguments(args, variables), piScript))
				return false;
		}

		if (bWithFile)
		{
			// use a temporary dest name
			String srcFileName = bufferData.GetDataFileAnsi(); // <-Call order is important
			String dstFileName = bufferData.GetDestFileName(); // <-Call order is important
			bHandled = plugin::InvokePackFile(srcFileName,
				dstFileName,
				bufferData.GetNChanged(),
				piScript, *itSubcode);
			if (bHandled)
				bufferData.ValidateNewFile();
		}
		else
		{
			bHandled = plugin::InvokePackBuffer(*bufferData.GetDataBufferAnsi(),
				bufferData.GetNChanged(),
				piScript, *itSubcode);
			if (bHandled)
				bufferData.ValidateNewBuffer();
		}

		// if this packer does not work, that is an error
		if (!bHandled)
			return false;

		// if the buffer changed, write it before leaving
		if (bufferData.GetNChangedValid() > 0)
		{
			bool bSuccess = bufferData.SaveAsFile(filepath);
			if (!bSuccess)
				return false;
		}
		++itSubcode;
	}
	return true;
}

bool PackingInfo::Packing(const String& srcFilepath, const String& dstFilepath, const std::vector<int>& handlerSubcodes, const std::vector<StringView>& variables) const
{
	String csTempFileName = srcFilepath;
	if (!Packing(csTempFileName, handlerSubcodes, variables))
		return false;
	try
	{
		TFile file1(csTempFileName);
		file1.copyTo(dstFilepath);
		if (srcFilepath!= csTempFileName)
			file1.remove();
		return true;
	}
	catch (Poco::Exception& e)
	{
		LogErrorStringUTF8(e.displayText());
		return false;
	}
}

bool PackingInfo::Unpacking(std::vector<int> * handlerSubcodes, String & filepath, const String& filteredText, const std::vector<StringView>& variables)
{
	// no handler : return true
	if (m_PluginPipeline.empty())
		return true;

	// control value
	String errorMessage;
	std::vector<std::tuple<PluginInfo*, std::vector<String>, bool>> plugins;
	if (!GetPackUnpackPlugin(filteredText, false, plugins, &m_PluginPipeline, errorMessage))
	{
		AppErrorMessageBox(errorMessage);
		return false;
	}

	for (auto& [plugin, args, bWithFile] : plugins)
	{
		if (plugin->m_argumentsRequired && args.empty())
		{

		}
	}

	if (handlerSubcodes)
		handlerSubcodes->clear();

	for (auto& [plugin, args, bWithFile] : plugins)
	{
		bool bHandled = false;
		storageForPlugins bufferData;
		bufferData.SetDataFileAnsi(filepath);

		// temporary subcode 
		int subcode = 0;

		LPDISPATCH piScript = plugin->m_lpDispatch;
		Poco::FastMutex::ScopedLock lock(g_mutex);

		if (plugin->m_hasVariablesProperty)
		{
			if (!plugin::InvokePutPluginVariables(String(variables[0].data(), variables[0].length()), piScript))
				return false;
		}
		if (plugin->m_hasArgumentsProperty)
		{
			if (!plugin::InvokePutPluginArguments(args.empty() ? plugin->m_arguments : MakeArguments(args, variables), piScript))
				return false;
		}

		if (bWithFile)
		{
			// use a temporary dest name
			bufferData.SetDestFileExtension(!plugin->m_ext.empty() ? plugin->m_ext : paths::FindExtension(filepath));
			String srcFileName = bufferData.GetDataFileAnsi(); // <-Call order is important
			String dstFileName = bufferData.GetDestFileName(); // <-Call order is important
			bHandled = plugin::InvokeUnpackFile(srcFileName,
				dstFileName,
				bufferData.GetNChanged(),
				piScript, subcode);
			if (bHandled)
				bufferData.ValidateNewFile();
		}
		else
		{
			bHandled = plugin::InvokeUnpackBuffer(*bufferData.GetDataBufferAnsi(),
				bufferData.GetNChanged(),
				piScript, subcode);
			if (bHandled)
				bufferData.ValidateNewBuffer();
		}

		// if this unpacker does not work, that is an error
		if (!bHandled)
			return false;

		// valid the subcode
		if (handlerSubcodes)
			handlerSubcodes->push_back(subcode);

		// if the buffer changed, write it before leaving
		if (bufferData.GetNChangedValid() > 0)
		{
			bool bSuccess = bufferData.SaveAsFile(filepath);
			if (!bSuccess)
				return false;
		}
	}
	return true;
}

String PackingInfo::GetUnpackedFileExtension(const String& filteredFilenames) const
{
	String ext;
	String errorMessage;
	std::vector<std::tuple<PluginInfo*, std::vector<String>, bool>> plugins;
	if (GetPackUnpackPlugin(filteredFilenames, false, plugins, nullptr, errorMessage))
	{
		for (auto& [plugin, args, bWithFile] : plugins)
			ext += plugin->m_ext;
	}
	return ext;
}

////////////////////////////////////////////////////////////////////////////////
// transformation prediffing

bool PrediffingInfo::GetPrediffPlugin(const String& filteredFilenames, bool bReverse,
	std::vector<std::tuple<PluginInfo*, std::vector<String>, bool>>& plugins,
	String *pPluginPipelineResolved, String& errorMessage) const
{
	auto result = ParsePluginPipeline(errorMessage);
	if (!errorMessage.empty())
		return false;
	std::vector<PluginForFile::PipelineItem> pipelineResolved;
	for (auto& [pluginName, args, quoteChar] : result)
	{
		PluginInfo* plugin = nullptr;
		bool bWithFile = true;
		if (pluginName == _T("<None>") || pluginName == _("<None>"))
			;
		else if (pluginName == _T("<Automatic>") || pluginName == _("<Automatic>"))
		{
			plugin = CAllThreadsScripts::GetActiveSet()->GetAutomaticPluginByFilter(L"FILE_PREDIFF", filteredFilenames);
			if (plugin == nullptr)
			{
				plugin = CAllThreadsScripts::GetActiveSet()->GetAutomaticPluginByFilter(L"BUFFER_PREDIFF", filteredFilenames);
				if (plugin)
					bWithFile = false;
			}
		}
		else
		{
			plugin = CAllThreadsScripts::GetActiveSet()->GetPluginByName(L"FILE_PREDIFF", pluginName);
			if (plugin == nullptr)
			{
				plugin = CAllThreadsScripts::GetActiveSet()->GetPluginByName(L"BUFFER_PREDIFF", pluginName);
				if (plugin == nullptr)
				{
					plugin = CAllThreadsScripts::GetActiveSet()->GetPluginByName(nullptr, pluginName);
					if (plugin == nullptr)
					{
						errorMessage = strutils::format_string1(_("Plugin not found or invalid: %1"), pluginName);
					}
					else
					{
						errorMessage = strutils::format(_T("'%s' is not PREDIFF plugin"), pluginName);
					}
					return false;
				}
				bWithFile = false;
			}
		}
		if (plugin)
		{
			pipelineResolved.push_back({ plugin->m_name, args, quoteChar });
			if (bReverse)
				plugins.insert(plugins.begin(), { plugin, args, bWithFile });
			else
				plugins.push_back({ plugin, args, bWithFile });
		}
	}
	if (pPluginPipelineResolved)
		*pPluginPipelineResolved = MakePluginPipeline(pipelineResolved);
	return true;
}

bool PrediffingInfo::Prediffing(String & filepath, const String& filteredText, bool bMayOverwrite, const std::vector<StringView>& variables)
{
	// no handler : return true
	if (m_PluginPipeline.empty())
		return true;

	// control value
	bool bHandled = false;
	String errorMessage;
	std::vector<std::tuple<PluginInfo*, std::vector<String>, bool>> plugins;
	if (!GetPrediffPlugin(filteredText, false, plugins, &m_PluginPipeline, errorMessage))
	{
		AppErrorMessageBox(errorMessage);
		return false;
	}

	for (const auto& [plugin, args, bWithFile] : plugins)
	{
		storageForPlugins bufferData;
		// detect Ansi or Unicode file
		bufferData.SetDataFileUnknown(filepath, bMayOverwrite);
		// TODO : set the codepage
		// bufferData.SetCodepage();

		LPDISPATCH piScript = plugin->m_lpDispatch;
		Poco::FastMutex::ScopedLock lock(g_mutex);

		if (plugin->m_hasVariablesProperty)
		{
			if (!plugin::InvokePutPluginVariables(String(variables[0].data(), variables[0].length()), piScript))
				return false;
		}
		if (plugin->m_hasArgumentsProperty)
		{
			if (!plugin::InvokePutPluginArguments(args.empty() ? plugin->m_arguments : MakeArguments(args, variables), piScript))
				return false;
		}

		if (bWithFile)
		{
			// use a temporary dest name
			String srcFileName = bufferData.GetDataFileAnsi(); // <-Call order is important
			String dstFileName = bufferData.GetDestFileName(); // <-Call order is important
			bHandled = plugin::InvokePrediffFile(srcFileName,
				dstFileName,
				bufferData.GetNChanged(),
				piScript);
			if (bHandled)
				bufferData.ValidateNewFile();
		}
		else
		{
			// probably it is for VB/VBscript so use a BSTR as argument
			bHandled = plugin::InvokePrediffBuffer(*bufferData.GetDataBufferUnicode(),
				bufferData.GetNChanged(),
				piScript);
			if (bHandled)
				bufferData.ValidateNewBuffer();
		}

		// if this unpacker does not work, that is an error
		if (!bHandled)
			return false;

		// if the buffer changed, write it before leaving
		if (bufferData.GetNChangedValid() > 0)
		{
			// bufferData changes filepath here to temp filepath
			bool bSuccess = bufferData.SaveAsFile(filepath);
			if (!bSuccess)
				return false;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// transformation text

bool EditorScriptInfo::GetEditorScriptPlugin(std::vector<std::tuple<PluginInfo*, std::vector<String>, int>>& plugins,
	String& errorMessage) const
{
	auto result = ParsePluginPipeline(errorMessage);
	if (!errorMessage.empty())
		return false;
	for (auto& [pluginName, args, quoteChar] : result)
	{
		bool found = false;
		PluginArray *pluginInfoArray = CAllThreadsScripts::GetActiveSet()->GetAvailableScripts(L"EDTIOR_SCRIPT");
		for (const auto& plugin : *pluginInfoArray)
		{
			std::vector<String> namesArray;
			std::vector<int> idArray;
			int nFunc = plugin::GetMethodsFromScript(plugin->m_lpDispatch, namesArray, idArray);
			for (int i = 0; i < nFunc; ++i)
			{
				if (namesArray[i] == pluginName)
				{
					plugins.push_back({ plugin.get(), args, idArray[i] });
					found = true;
					break;
				}
			}
			if (found)
				break;
		}
		if (!found)
		{
			errorMessage = strutils::format_string1(_("Plugin not found or invalid: %1"), pluginName);
			return false;
		}
	}
	return true;
}

bool EditorScriptInfo::Transform(String & text, const std::vector<StringView>& variables)
{
	// no handler : return true
	if (m_PluginPipeline.empty())
		return true;

	// control value
	bool bHandled = false;
	String errorMessage;
	std::vector<std::tuple<PluginInfo*, std::vector<String>, int>> plugins;
	if (!GetEditorScriptPlugin(plugins, errorMessage))
	{
		AppErrorMessageBox(errorMessage);
		return false;
	}

	for (const auto& [plugin, args, fncID] : plugins)
	{
		LPDISPATCH piScript = plugin->m_lpDispatch;
		Poco::FastMutex::ScopedLock lock(g_mutex);

		if (plugin->m_hasVariablesProperty)
		{
			if (!plugin::InvokePutPluginVariables(String(variables[0].data(), variables[0].length()), piScript))
				return false;
		}
		if (plugin->m_hasArgumentsProperty)
		{
			if (!plugin::InvokePutPluginArguments(args.empty() ? plugin->m_arguments : MakeArguments(args, variables), piScript))
				return false;
		}

		// execute the transform operation
		int nChanged = 0;
		if (!plugin::InvokeTransformText(text, nChanged, plugin->m_lpDispatch, fncID))
			return false;
	}
	return true;
}

namespace FileTransform
{

bool AutoUnpacking = false;
bool AutoPrediffing = false;

////////////////////////////////////////////////////////////////////////////////

bool AnyCodepageToUTF8(int codepage, String & filepath, bool bMayOverwrite)
{
	String tempDir = env::GetTemporaryPath();
	if (tempDir.empty())
		return false;
	String tempFilepath = env::GetTemporaryFileName(tempDir, _T("_W3"));
	if (tempFilepath.empty())
		return false;
	// TODO : is it better with the BOM or without (just change the last argument)
	int nFileChanged = 0;
	bool bSuccess = ::AnyCodepageToUTF8(codepage, filepath, tempFilepath, nFileChanged, false); 
	if (bSuccess && nFileChanged!=0)
	{
		// we do not overwrite so we delete the old file
		if (bMayOverwrite)
		{
			try
			{
				TFile(filepath).remove();
			}
			catch (Exception& e)
			{
				LogErrorStringUTF8(e.displayText());
			}
		}
		// and change the filepath if everything works
		filepath = tempFilepath;
	}
	else
	{
		try
		{
			TFile(tempFilepath).remove();
		}
		catch (Exception& e)
		{
			LogErrorStringUTF8(e.displayText());
		}
	}

	return bSuccess;
}


////////////////////////////////////////////////////////////////////////////////
// transformation : TextTransform_Interactive (editor scripts)

bool Interactive(String & text, const std::vector<String>& args, const wchar_t *TransformationEvent, int iFncChosen, const std::vector<StringView>& variables)
{
	if (iFncChosen < 0)
		return false;

	// get an array with the available scripts
	PluginArray * piScriptArray = 
		CAllThreadsScripts::GetActiveSet()->GetAvailableScripts(TransformationEvent);

	size_t iScript;
	for (iScript = 0 ; iScript < piScriptArray->size() ; iScript++)
	{
		if (iFncChosen < piScriptArray->at(iScript)->m_nFreeFunctions)
			// we have found the script file
			break;
		iFncChosen -= piScriptArray->at(iScript)->m_nFreeFunctions;
	}

	if (iScript >= piScriptArray->size())
		return false;

	PluginInfo* plugin = piScriptArray->at(iScript).get();

	// iFncChosen is the index of the function in the script file
	// we must convert it to the function ID
	int fncID = plugin::GetMethodIDInScript(plugin->m_lpDispatch, iFncChosen);

	if (plugin->m_hasVariablesProperty)
	{
		if (!plugin::InvokePutPluginVariables(String(variables[0].data(), variables[0].length()), plugin->m_lpDispatch))
			return false;
	}
	if (plugin->m_hasArgumentsProperty)
	{
		if (!plugin::InvokePutPluginArguments(args.empty() ? plugin->m_arguments : PluginForFile::MakeArguments(args, variables), plugin->m_lpDispatch))
			return false;
	}

	// execute the transform operation
	int nChanged = 0;
	plugin::InvokeTransformText(text, nChanged, plugin->m_lpDispatch, fncID);

	return (nChanged != 0);
}

std::pair<
	std::vector<std::tuple<String, String, unsigned, PluginInfo *>>,
	std::map<String, std::vector<std::tuple<String, String, unsigned, PluginInfo *>>>
>
CreatePluginMenuInfos(const String& filteredFilenames, const std::vector<std::wstring>& events, unsigned baseId)
{
	std::vector<std::tuple<String, String, unsigned, PluginInfo *>> suggestedPlugins;
	std::map<String, std::vector<std::tuple<String, String, unsigned, PluginInfo *>>> allPlugins;
	std::map<String, int> captions;
	unsigned id = baseId;
	bool addedNoneAutomatic = false;
	static PluginInfo noPlugin;
	static PluginInfo autoPlugin;
	if (autoPlugin.m_name.empty())
		autoPlugin.m_name = _T("<Automatic>");
	for (const auto& event: events)
	{
		auto pScriptArray =
			CAllThreadsScripts::GetActiveSet()->GetAvailableScripts(event.c_str());
		for (auto& plugin : *pScriptArray)
		{
			if (!plugin->m_disabled)
			{
				if (event != L"EDITOR_SCRIPT")
				{
					if (!addedNoneAutomatic)
					{
						String process = _T("");
						allPlugins.insert_or_assign(process, std::vector<std::tuple<String, String, unsigned, PluginInfo *>>());
						allPlugins[process].emplace_back(_("<None>"), _T(""), id++, &noPlugin);
						allPlugins[process].emplace_back(_("<Automatic>"), _T("<Automatic>"), id++, &autoPlugin);
						addedNoneAutomatic = true;
					}
					const auto menuCaption = plugin->GetExtendedPropertyValue(_T("MenuCaption"));
					const auto processType = plugin->GetExtendedPropertyValue(_T("ProcessType"));
					const String caption = tr(ucr::toUTF8(menuCaption.has_value() ?
						String{ menuCaption->data(), menuCaption->size() } : plugin->m_name));
					const String process = tr(ucr::toUTF8(processType.has_value() ?
						String{ processType->data(), processType->size() } : _T("&Others")));

					if (plugin->TestAgainstRegList(filteredFilenames))
						suggestedPlugins.emplace_back(caption, plugin->m_name, id, plugin.get());

					if (allPlugins.find(process) == allPlugins.end())
						allPlugins.insert_or_assign(process, std::vector<std::tuple<String, String, unsigned, PluginInfo *>>());
					allPlugins[process].emplace_back(caption, plugin->m_name, id, plugin.get());
					captions[caption]++;
					id++;
				}
				else
				{
					LPDISPATCH piScript = plugin->m_lpDispatch;
					std::vector<String> scriptNamesArray;
					std::vector<int> scriptIdsArray;
					int nScriptFnc = plugin::GetMethodsFromScript(piScript, scriptNamesArray, scriptIdsArray);
					bool matched = plugin->TestAgainstRegList(filteredFilenames);
					for (int i = 0; i < nScriptFnc; ++i, ++id)
					{
						const auto menuCaption = plugin->GetExtendedPropertyValue(scriptNamesArray[i] + _T(".MenuCaption"));
						auto processType = plugin->GetExtendedPropertyValue(scriptNamesArray[i] + _T(".ProcessType"));
						if (!processType.has_value())
							processType = plugin->GetExtendedPropertyValue(_T("ProcessType"));
						const String caption = tr(ucr::toUTF8(menuCaption.has_value() ?
							String{ menuCaption->data(), menuCaption->size() } : scriptNamesArray[i]));
						const String process = tr(ucr::toUTF8(processType.has_value() ?
							String{ processType->data(), processType->size() } : _T("&Others")));
						if (matched)
							suggestedPlugins.emplace_back(caption, plugin->m_name, id, plugin.get());
						if (allPlugins.find(process) == allPlugins.end())
							allPlugins.insert_or_assign(process, std::vector<std::tuple<String, String, unsigned, PluginInfo *>>());
						allPlugins[process].emplace_back(caption, plugin->m_name, id, plugin.get());
					}
				}
			}
		}
	}
	auto ResolveConflictMenuCaptions = [&captions](auto& plugins)
	{
		for (auto& plugin : plugins)
		{
			const String& caption = std::get<0>(plugin);
			if (captions[caption] > 1)
				std::get<0>(plugin) = caption + _T("(") + std::get<1>(plugin) + _T(")");
		}
	};
	ResolveConflictMenuCaptions(suggestedPlugins);
	for (auto& [processType, plugins] : allPlugins)
		ResolveConflictMenuCaptions(plugins);
	return { suggestedPlugins, allPlugins };
}

}

////////////////////////////////////////////////////////////////////////////////

/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
/**
***********************************************************************************************************************
* @file  settingsServer.h
* @brief Class declaration for SettingServer.
***********************************************************************************************************************
*/

#pragma once

#include "settingsProtocol.h"
#include "baseProtocolServer.h"
#include "util/vector.h"

namespace DevDriver
{
    namespace SettingsProtocol
    {
        class SettingsServer : public BaseProtocolServer
        {
        public:
            explicit SettingsServer(IMsgChannel* pMsgChannel);
            ~SettingsServer();

            void Finalize() override;

            bool AcceptSession(const SharedPointer<ISession>& pSession) override;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override;
            void UpdateSession(const SharedPointer<ISession>& pSession) override;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

            void AddCategory(const char* pName, const char* pParentName);
            int32 QueryCategoryIndex(const char* pName) const;

            void AddSetting(const Setting* pSetting);
            bool QuerySetting(const char* pName, Setting* pSetting);
            bool QuerySettingByIndex(uint32 settingIndex, Setting* pSetting);
            bool UpdateSetting(const char* pName, const SettingValue* pValue);
            bool UpdateSettingByIndex(uint32 settingIndex, const SettingValue* pValue);

            uint32 GetNumSettings();
            uint32 GetNumCategories();

        private:
            void LockData();
            void UnlockData();

            int32 FindCategory(const char* pCategoryName) const;
            int32 FindSetting(const char* pSettingName) const;

            Vector<Setting, 8> m_settings;
            Vector<SettingCategory, 8> m_categories;
            Platform::Mutex m_mutex;
        };
    }
} // DevDriver

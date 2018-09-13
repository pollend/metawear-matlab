/**
* Copyright 2018 GaitRehabilitation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <MexUtility.h>
#include <regex>
#include "handlers/ConnectionHandler.h"
#include "FunctionWrapper.h"
#include "MetawearWrapper.h"
#include "MexPrintStream.h"

ConnectionHandler::ConnectionHandler(FunctionWrapper* wrapper): m_devices() {
    std::map<std::string, WrapperMethod *> functions = {
            {"connect",    mexConnect},
            {"disconnect", mexDisconnect}
    };
    wrapper->registerMethod(this, functions);
}

ConnectionHandler::~ConnectionHandler() {

}

MetawearWrapper* ConnectionHandler::getDevice(const std::string& mac){
    auto  it = m_devices.find(mac);
    if (it != m_devices.end()) {
        return it->second;
    }
    return nullptr;
}

void ConnectionHandler::addDevice(MetawearWrapper* wrapper){
    m_devices.emplace(wrapper->getMacAddress(),wrapper);
}

MetawearWrapper* ConnectionHandler::removeDevice(const std::string& mac) {
    auto item = getDevice(mac);
    if (item != nullptr)
        m_devices.erase(mac);
    return item;

}

 void ConnectionHandler::mexConnect(std::shared_ptr<matlab::engine::MATLABEngine> engine,
         void *context,  ParameterWrapper& outputs, ParameterWrapper& inputs) {
     auto *handler = static_cast<ConnectionHandler *>(context);

     if (inputs.size() != 2)
         MexUtility::error(engine, "Two Inputs Required");
     if (outputs.size() != 1)
         MexUtility::error(engine, "Single argument for output");
     if (inputs[1].getType() != matlab::data::ArrayType::CHAR)
         MexUtility::error(engine, "Second Parameter has to be string type");

     matlab::data::CharArray address = inputs[1];

     // standard IEEE 802 format for MAC-48 address. verify if the format is correct.
     std::regex addressMatch("^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$");
     if (!std::regex_match(address.toAscii(), addressMatch)) {
         MexUtility::error(engine, "Invalid bluetooth address for second parameter");
         return;
     }

     if(handler->getDevice(address.toAscii()) != nullptr){
         matlab::data::ArrayFactory factory;
         matlab::data::CharArray addressCharArray = factory.createCharArray(address.toAscii());
         outputs[0] = addressCharArray;
         MexUtility::printf(engine,"Already Connected \n");
         return;
     }

     MetawearWrapper *wrapper = new MetawearWrapper(address.toAscii(),engine);
     unsigned int lattemptTime = 200;
     while (true) {
         wrapper->connect();
         wrapper->mexStreamBlock();
         if (wrapper->isConnected()) {
             handler->addDevice(wrapper);
             break;
         }
         if (lattemptTime > 5000) break;
         MexUtility::printf(engine,"Attempting in " + std::to_string(lattemptTime) + " milliseconds \n");
         std::this_thread::sleep_for(std::chrono::milliseconds(lattemptTime));
         lattemptTime *= 2;
     }
     if (wrapper->isConnected()) {
         MexUtility::printf(engine,"Connection Established \n");
         matlab::data::ArrayFactory factory;
         matlab::data::CharArray addressCharArray = factory.createCharArray(wrapper->getMacAddress());
         outputs[0] = addressCharArray;
     } else {
         free(wrapper);
         matlab::data::ArrayFactory factory;
         matlab::data::CharArray addressCharArray = factory.createCharArray("FF:FF:FF:FF:FF:FF");
         outputs[0] = addressCharArray;
     }
 }

void ConnectionHandler::mexDisconnect( std::shared_ptr<matlab::engine::MATLABEngine> engine,void *context,  ParameterWrapper& outputs, ParameterWrapper& inputs){
    ConnectionHandler* handler = static_cast<ConnectionHandler*>(context);
    if (inputs.size() != 2) {
        MexUtility::error(engine, "Two Inputs Required");
        return;
    }
    matlab::data::CharArray address =  inputs[1];
    MetawearWrapper* wrapper =  handler->getDevice(address.toAscii());

    wrapper->disconnect();
    wrapper->mexStreamBlock();
}

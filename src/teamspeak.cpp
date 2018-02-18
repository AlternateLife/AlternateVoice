/*
 * File: src/teamspeak.cpp
 * Date: 08.02.2018
 *
 * MIT License
 *
 * Copyright (c) 2018 JustAnotherVoiceChat
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "teamspeak.h"

#include "teamspeakPlugin.h"
#include "util.h"

#define BUFFER_LENGTH 256

static uint64 serverConnectionHandler = 0;
static std::set<anyID> mutedClients;

void ts3_log(std::string message, enum LogLevel severity) {
  ts3Functions.logMessage(message.c_str(), severity, "JustAnotherVoiceChat", 0);
}

bool ts3_connect(std::string host, uint16_t port, std::string serverPassword) {
  auto desiredIp = resolveHostname(host);
  if (desiredIp.compare("") == 0) {
    ts3_log("Unable to resolve desired hostname: " + host, LogLevel_WARNING);
    return false;
  }

  ts3_log(std::string("Desired ip: ") + desiredIp + " for " + host, LogLevel_DEBUG);

  // check if server is already connected
  uint64 *serverList;
  int result = ts3Functions.getServerConnectionHandlerList(&serverList);
  if (result != ERROR_ok) {
    ts3_log("Unable to get server list", LogLevel_ERROR);
    return false;
  }

  // search for host in list, skip entry because it is 1 every time
  int index = 0;
  uint64 handle = serverList[index];

  while (handle != 0) {
    char handleHost[BUFFER_LENGTH] = { '\0' };
    char password[BUFFER_LENGTH] = { '\0' };
    unsigned short handlePort = 0;

    result = ts3Functions.getServerConnectInfo(handle, handleHost, &handlePort, password, BUFFER_LENGTH);
    if (result == ERROR_ok) {
      // resolve hostname
      auto ip = resolveHostname(std::string(handleHost));
      if (desiredIp.compare(ip) == 0) {
        ts3_log(std::string("Matching server found: ") + handleHost, LogLevel_DEBUG);

        // save handle
        serverConnectionHandler = handle;
        break;
      }
    } else {
      ts3_log("Unable to get server connection status " + std::to_string(handle), LogLevel_ERROR);
    }

    // get next handle
    index++;
    handle = serverList[index];
  }

  // server list needs to be freeded after usage
  ts3Functions.freeMemory(serverList);

  if (serverConnectionHandler != 0) {
    return true;
  }

  // create connection handler if needed
  result = ts3Functions.spawnNewServerConnectionHandler(0, &serverConnectionHandler);
  if (result != ERROR_ok) {
    ts3_log("Unable to spawn server connection handler", LogLevel_ERROR);
    return false;
  }

  // char *identity;
  // result = ts3Functions.createIdentity(&identity);
  // if (result != ERROR_ok) {
  //   ts3_log("Unable to create a teamspeak identity", LogLevel_ERROR);
  //   return false;
  // }

  // result = ts3Functions.startConnection(serverConnectionHandler, identity, host.c_str(), port, "Test", NULL, "", serverPassword.c_str());
  // if (result != ERROR_ok) {
  //   ts3_log(std::to_string(result) + ": Unable to connect to " + host + ":" + std::to_string(port), LogLevel_WARNING);
  //   return false;
  // }

  // ts3Functions.freeMemory(identity);
  return false;
}

void ts3_disconnect() {
  if (serverConnectionHandler != 0) {
    ts3Functions.destroyServerConnectionHandler(serverConnectionHandler);
  }
}

bool ts3_moveToChannel(uint64 channelId, std::string password) {
  // get client id
  auto clientId = ts3_clientId(serverConnectionHandler);
  if (clientId == 0) {
    ts3_log("Unable to get client id for channel move", LogLevel_WARNING);
    return false;
  }

  auto result = ts3Functions.requestClientMove(serverConnectionHandler, clientId, channelId, password.c_str(), NULL);
  if (result != ERROR_ok) {
    ts3_log("Unable to move into the channel " + channelId, LogLevel_WARNING);
    return false;
  }

  return true;
}

bool ts3_muteClient(anyID clientId, bool mute) {
  std::set<anyID> clients;
  clients.insert(clientId);

  return ts3_muteClients(clients, mute);
}

bool ts3_muteClients(std::set<anyID> &clients, bool mute) {
  if (clients.empty()) {
    return true;
  }

  // create client list
  anyID *clientIds = (anyID *)malloc((clients.size() + 1) * sizeof(anyID));

  int index = 0;
  for (auto it = clients.begin(); it != clients.end(); it++) {
    clientIds[index++] = *it;
  }

  // terminate array with zero element
  clientIds[index] = 0;

  // apply (un-)mute on clients
  unsigned int result;

  if (mute) {
    result = ts3Functions.requestMuteClients(serverConnectionHandler, clientIds, NULL);
    if (result == ERROR_ok) {
      // add all new clients to cached list
      for (auto it = clients.begin(); it != clients.end(); it++) {
        mutedClients.insert(*it);
      }
    }
  } else {
    result = ts3Functions.requestUnmuteClients(serverConnectionHandler, clientIds, NULL);
    if (result == ERROR_ok) {
      // remove all clients from cached list
      for (auto it = clients.begin(); it != clients.end(); it++) {
        anyID clientId = *it;
        auto eraseIt = mutedClients.begin();

        while (eraseIt != mutedClients.end()) {
          if (*eraseIt == clientId) {
            eraseIt = mutedClients.erase(eraseIt);
          } else {
            eraseIt++;
          }
        }
      }
    }
  }

  // cleanup list
  free(clientIds);

  return result == ERROR_ok;
}

bool ts3_unmuteAllClients() {
  return ts3_muteClients(mutedClients, false);
}

std::set<anyID> ts3_clientsInChannel(uint64 channelId) {
  anyID *clientList;
  std::set<anyID> clients;

  auto result = ts3Functions.getChannelClientList(serverConnectionHandler, channelId, &clientList);
  if (result != ERROR_ok) {
    return clients;
  }

  // put client ids into the set
  int index = 0;
  anyID id = clientList[index];

  while (id != 0) {
    clients.insert(id);

    index++;
    id = clientList[index];
  }

  ts3Functions.freeMemory(clientList);
  return clients;
}

uint64 ts3_serverConnectionHandle() {
  return serverConnectionHandler;
}

anyID ts3_clientId(uint64 serverConnectionHandlerId) {
  // check if connected to the server
  if (serverConnectionHandlerId == 0) {
    ts3_log("Unable to get client ID when not connected to a server", LogLevel_WARNING);
    return 0;
  }

  int status;
  int result = ts3Functions.getConnectionStatus(serverConnectionHandlerId, &status);
  if (result != ERROR_ok) {
    ts3_log("Unable to get server connection status", LogLevel_WARNING);
    return 0;
  }

  // 1 = connected, 0 = not connected
  if (status <= 0) {
    ts3_log("Not connected to the server " + std::to_string(status), LogLevel_DEBUG);
    return 0;
  }

  // get client ID on this server
  anyID clientID;
  result = ts3Functions.getClientID(serverConnectionHandlerId, &clientID);
  if (result != ERROR_ok) {
    ts3_log("Unable to get client ID", LogLevel_ERROR);
    return 0;
  }

  return clientID;
}

uint64 ts3_channelId(uint64 serverConnectionHandlerId) {
  uint64 channelId;
  auto clientId = ts3_clientId(serverConnectionHandler);

  auto result = ts3Functions.getChannelOfClient(serverConnectionHandlerId, clientId, &channelId);
  if (result != ERROR_ok) {
    return 0;
  }

  return channelId;
}

void ts3_setClientVolumeModifier(anyID clientID, float value) {

}

void ts3_setClientPosition(anyID clientID, const struct TS3_Vector *position) {

}

#pragma once

#include "network_config.h"

#include <PubSubClient.h>

bool webPortalIsApMode();
bool webPortalIsWifiConnected();
const NetworkSettings &webPortalNetworkSettings();

void webPortalBegin(const NetworkSettings &defaults);
void webPortalLoop();
void webPortalRequestMqttReconnect();
void webPortalRequestMqttRepublish();
bool webPortalConsumeMqttRepublishRequest();
void webPortalSetMqttConnected(bool connected);

bool webPortalConnectMqtt(PubSubClient &mqtt, const char *deviceId,
                          const char *statusTopic);

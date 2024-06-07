#include "BinsentryCommBinS3PresignedURLFeature.h"
#include "../../../logging/LoggerFactory.h"
#include "../../../util/FileUtils.h"
#include "binsentry-obj-manager-server.h"

#include <aws/common/byte_buf.h>
#include <aws/crt/Api.h>
#include <aws/iotdevicecommon/IotDevice.h>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <utility>


using namespace std;
using namespace Aws;
using namespace Aws::Iot;
using namespace Aws::Crt;
using namespace Aws::Iot::DeviceClient;
using namespace Aws::Crt::Mqtt;
using namespace Aws::Iot::DeviceClient::Custom;
using namespace Aws::Iot::DeviceClient::Custom::Binsentry;
using namespace Aws::Iot::DeviceClient::Util;
using namespace Aws::Iot::DeviceClient::Logging;

using namespace org::binsentry::S3PresignedURL;

constexpr size_t MAX_IOT_CORE_MQTT_MESSAGE_SIZE_BYTES = 128000;


string S3PresignedURLFeature::getName()
{
    return NAME;
}

int S3PresignedURLFeature::init(
    shared_ptr<SharedCrtResourceManager> manager,
    shared_ptr<ClientBaseNotifier> notifier,
    const PlainConfig &config)
{
    resourceManager = manager;
    baseNotifier = notifier;
    thingName = *config.thingName;

    // FUTURE: Take stage from config (eg. "p")
    pubTopic = "sensor/" + thingName + "/p/hdf5/v1/url/get";
    subTopic = pubTopic + "/accepted";

    return AWS_OP_SUCCESS;
}

int S3PresignedURLFeature::publishS3PresignedURLRequest(unsigned int requestId, int64_t timeout_ms)
{
    // only way to re-try subscribe is when external request comes through
    // also not point in publishing if we don't have a subscription to the response topic
    if (!hasSubscribedToS3PresignedURLResponse) {
        subscribeToS3PresignedURLResponse();
        if (!hasSubscribedToS3PresignedURLResponse) {
            return AWS_OP_ERR;
        }
    }

    ByteBuf payload;
    std::string jsonRequest = "{\"requestId\": " + std::to_string(requestId) + "}";
    int payloadInitResult = aws_byte_buf_init(&payload, resourceManager->getAllocator(), jsonRequest.length());
    if (payloadInitResult != AWS_OP_SUCCESS) {
        return payloadInitResult;
    }

    std::copy(jsonRequest.begin(), jsonRequest.end(), reinterpret_cast<char *>(payload.buffer));
    payload.len = jsonRequest.length();

    condition_variable cvLambdaDone;
    mutex mtxLambdaDone;
    bool finished = false;
    int publishErrorCode = AWS_OP_ERR;
    auto onPublishComplete = [payload, &cvLambdaDone, &mtxLambdaDone, &finished, &publishErrorCode, this](const Mqtt::MqttConnection &, uint16_t packetId, int errorCode) mutable {
        try {   // NOTE: std::system_error can occur when aws-iot-device-client is recovering from MQTT connection lost
            std::lock_guard<std::mutex> lk(mtxLambdaDone);
        } catch (std::system_error& e) {
            return;
        }

        LOGM_DEBUG(TAG, "PublishCompAck: Name:(%s), PacketId:(%hu), ErrorCode:%d", getName().c_str(), packetId, errorCode);
        finished = true;
        publishErrorCode = errorCode;
        cvLambdaDone.notify_all();
    };
    uint16_t publishPacketId = resourceManager->getConnection()->Publish(
        pubTopic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, false, payload, onPublishComplete);
    if (publishPacketId == 0) {
        LOGM_ERROR(TAG, "Publish failed synchronously: Name:(%s)", getName().c_str());
        aws_byte_buf_clean_up_secure(&payload);
        return AWS_OP_ERR;
    }

    unique_lock<mutex> lk(mtxLambdaDone);
    if (cvLambdaDone.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&]{return finished;})) {
        aws_byte_buf_clean_up_secure(&payload);
        return publishErrorCode;
    } else {
        LOGM_ERROR(TAG, "PublishCompAck timeout: Name:(%s)", getName().c_str());
        aws_byte_buf_clean_up_secure(&payload);
        return AWS_OP_ERR;
    }
}

void S3PresignedURLFeature::onSubscribeReceiveS3PresignedURLResponse(const MqttConnection &connection, const String &topic, const ByteBuf &payload, bool dup, QOS qos, bool retain) {
    if (qos != AWS_MQTT_QOS_AT_LEAST_ONCE) {
        LOGM_WARN(TAG, "SubReceive QOS unexpected: QOS: %d", qos);
    }

    if (dup) {
        LOGM_INFO(TAG, "SubReceive potential duplicate: Topic: '%s'", topic.c_str());
    }

    if (retain) {
        LOGM_INFO(TAG, "SubReceive retained message unexpected: Topic: '%s'", topic.c_str());
    }

    if (subTopic.compare(topic) != 0) {
        LOGM_ERROR(TAG, "SubReceive topic does not match expected: Topic: '%s'", topic.c_str());
        return;
    }

    if (payload.len == 0)
    {
        LOGM_WARN(TAG, "SubReceive empty payload: Topic: '%s'", topic.c_str());
        return;
    }

    // Examples of the response types for the /accepted MQTT topic response will look like
    //{
    //    "requestId": <number>,
    //    "presignedPutUrl": <string>,
    //    "secondsUntilExpiry": <number>,
    //    "timestamp": <number>, // Unix timestamp (including ms) of the time we received the request for the pre-signed URL
    //}
    //or
    //{
    //    "requestId": <number>,
    //    "error": {
    //        "code": <number - 0 if unknown>,
    //        "message": <string>
    //    },
    //    "timestamp": <number>, // Unix timestamp (including ms) of the time we received the request for the pre-signed URL
    // }

    const std::basic_string<char, std::char_traits<char>, StlAllocator<char>> jsonString(reinterpret_cast<char *>(payload.buffer), payload.len);

    uint16_t requestId = 0;
    int32_t returnCode = -1;
    std::string responseJSONString;

    auto jsonObj = Aws::Crt::JsonObject(jsonString);
    if (jsonObj.WasParseSuccessful()) {
        auto jsonView = Aws::Crt::JsonView(jsonObj);
        const char *jsonKey = "requestId";
        bool requestIDExists = jsonView.ValueExists(jsonKey);
        if (requestIDExists)
        {
            int jsonRequestId = jsonView.GetInteger(jsonKey);
            if (jsonRequestId > 0 && jsonRequestId <= UINT16_MAX) {
                requestId = (uint16_t)jsonRequestId;
            }
        }

        jsonKey = "error";
        if (jsonView.ValueExists(jsonKey))
        {
            // have error
            JsonView jsonView2 = jsonView.GetJsonObject(jsonKey);

            int jsonErrorCode = 0;  // default to unknown error code
            std::string message;

            const char *jsonKey2 = "code";
            if (jsonView2.ValueExists(jsonKey2))
            {
                jsonErrorCode = jsonView2.GetInteger(jsonKey2);
                if (jsonErrorCode < 0 || jsonErrorCode > 1)
                {
                    returnCode = -(int32_t)jsonErrorCode;
                }
            }

            jsonKey2 = "message";
            if (jsonView2.ValueExists(jsonKey2))
            {
                message = jsonView2.GetString(jsonKey2);
            }

            LOGM_ERROR(TAG, "SubReceive error: %d, '%s'", jsonErrorCode, message.c_str());
        } else {
            jsonKey = "presignedPutUrl";
            bool presignedPutUrlExists = jsonView.ValueExists(jsonKey);
            if (presignedPutUrlExists)
            {
                responseJSONString = jsonView.GetString(jsonKey);
            }

            if (requestIDExists && presignedPutUrlExists) {
                returnCode = 0;
            }
        }
    } else {
        LOGM_ERROR(TAG, "Couldn't parse JSON payload. GetErrorMessage returns: %s", jsonObj.GetErrorMessage().c_str());
    }

    if (dbusS3PresignedURL != nullptr) {
        dbusS3PresignedURL->emitS3PreSignedURLResponse(requestId, returnCode, responseJSONString);
    }
}

int S3PresignedURLFeature::subscribeToS3PresignedURLResponse(int64_t timeout_ms) {
    condition_variable cvLambdaDone;
    mutex mtxLambdaDone;
    bool finished = false;
    int subscribeErrorCode = AWS_OP_ERR;

    auto onSubAck = [&cvLambdaDone, &mtxLambdaDone, &finished, &subscribeErrorCode, this](const MqttConnection &, uint16_t packetId, const String &topic, QOS, int errorCode) -> void {
        LOGM_DEBUG(TAG, "SubAck: Name:(%s), PacketId:(%hu), ErrorCode:%d", getName().c_str(), packetId, errorCode);
        std::lock_guard<std::mutex> lk(mtxLambdaDone);
        finished = true;
        subscribeErrorCode = errorCode;
        cvLambdaDone.notify_all();
    };
    auto onRecvData = [this](const MqttConnection &connection, const String &topic, const ByteBuf &payload, bool dup, QOS qos, bool retain) -> void {
        LOGM_DEBUG(TAG, "Message received on subscribe topic (%s), size: %zu bytes", topic.c_str(), payload.len);
        this->onSubscribeReceiveS3PresignedURLResponse(connection, topic, payload, dup, qos, retain);
    };

    uint16_t subscribePacketId = resourceManager->getConnection()->Subscribe(
        subTopic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, onRecvData, onSubAck);
    if (subscribePacketId == 0) {
        LOGM_ERROR(TAG, "Subscribe failed synchronously: Name:(%s), Topic:'%s'", getName().c_str(), subTopic.c_str());
        return AWS_OP_ERR;
    }

    unique_lock<mutex> lk(mtxLambdaDone);
    if (cvLambdaDone.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&]{return finished;})) {
        if (subscribeErrorCode == AWS_OP_SUCCESS) {
            hasSubscribedToS3PresignedURLResponse = true;
        }

        return subscribeErrorCode;
    } else {
        LOGM_ERROR(TAG, "SubAck timeout: Name:(%s)", getName().c_str());
        return AWS_OP_ERR;
    }
}

int S3PresignedURLFeature::subscribeToS3PresignedURLResponse(int64_t timeout_ms, int retries) {
    if (retries < 0) {
        retries = 0;
    }

    for (int i = 0; i < retries + 1; i++) {
        int result = subscribeToS3PresignedURLResponse(timeout_ms);
        if (result == AWS_OP_SUCCESS) {
            return result;
        }

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(5s);
    }

    return AWS_OP_ERR;
}

void S3PresignedURLFeature::subscribeToS3PresignedURLResponse() {
    int subscribeResult = subscribeToS3PresignedURLResponse(1000LL * 60LL * 2LL /* timeout_ms= */, 100 /* retries= */);
    if (subscribeResult != AWS_OP_SUCCESS) {
        LOGM_ERROR(TAG, "Failed to subscribe to S3 pre-signed URL response topic: Name:(%s)", getName().c_str());
    }
}

int S3PresignedURLFeature::start()
{
    LOGM_INFO(TAG, "Starting %s", getName().c_str());

    subscribeToS3PresignedURLResponse();

    // FUTURE: Add mechanism to check D-Bus initialized correctly and if not try again
    setupDBus();

    baseNotifier->onEvent(static_cast<Feature *>(this), ClientBaseEventNotification::FEATURE_STARTED);
    return AWS_OP_SUCCESS;
}

void S3PresignedURLFeature::setupDBus() {
    std::lock_guard<std::mutex> lock(dbusLock);

    dbusConnection = sdbus::createSessionBusConnection();
    dbusConnection->requestName(DBUS_BUS_NAME);
    dbusConnection->enterEventLoopAsync();

    dbusManager = std::make_unique<ManagerAdaptor>(*dbusConnection, DBUS_PATH_BASE_NAME);
    auto s3UrlRequestHandler = [this](uint16_t requestId) -> int32_t {
        return (int32_t)this->publishS3PresignedURLRequest(requestId, 300);
    };
    dbusS3PresignedURL = std::make_unique<S3PresignedURLAdaptor>(*dbusConnection, DBUS_PATH_NAME, "hdf5", s3UrlRequestHandler);

    if (!isDBusSetup()) {
        cleanupDBus();
    }
}

void S3PresignedURLFeature::cleanupDBus() {
    std::lock_guard<std::mutex> lock(dbusLock);

    if (dbusS3PresignedURL != nullptr) {
        dbusS3PresignedURL = nullptr;
    }

    if (dbusManager != nullptr) {
        dbusManager = nullptr;
    }

    if (dbusConnection != nullptr)
    {
        dbusConnection->releaseName(DBUS_BUS_NAME);
        dbusConnection->leaveEventLoop();
        dbusConnection = nullptr;
    }
}

bool S3PresignedURLFeature::isDBusSetup() {
    std::lock_guard<std::mutex> lock(dbusLock);
    return (dbusConnection != nullptr && dbusManager != nullptr && dbusS3PresignedURL != nullptr);
}

int S3PresignedURLFeature::stop()
{
    needStop.store(true);

    auto onUnsubscribe = [](const MqttConnection &, uint16_t packetId, int errorCode) -> void {
        LOGM_DEBUG(TAG, "Unsubscribing: PacketId:%u, ErrorCode:%d", packetId, errorCode);
    };

    cleanupDBus();

    resourceManager->getConnection()->Unsubscribe(subTopic.c_str(), onUnsubscribe);
    baseNotifier->onEvent(static_cast<Feature *>(this), ClientBaseEventNotification::FEATURE_STOPPED);
    return AWS_OP_SUCCESS;
}